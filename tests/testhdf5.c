/*
Copyright (c) 1994 - 2010, Lawrence Livermore National Security, LLC.
LLNL-CODE-425250.
All rights reserved.

This file is part of Silo. For details, see silo.llnl.gov.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the disclaimer below.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the disclaimer (as noted
     below) in the documentation and/or other materials provided with
     the distribution.
   * Neither the name of the LLNS/LLNL nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

THIS SOFTWARE  IS PROVIDED BY  THE COPYRIGHT HOLDERS  AND CONTRIBUTORS
"AS  IS" AND  ANY EXPRESS  OR IMPLIED  WARRANTIES, INCLUDING,  BUT NOT
LIMITED TO, THE IMPLIED  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A  PARTICULAR  PURPOSE ARE  DISCLAIMED.  IN  NO  EVENT SHALL  LAWRENCE
LIVERMORE  NATIONAL SECURITY, LLC,  THE U.S.  DEPARTMENT OF  ENERGY OR
CONTRIBUTORS BE LIABLE FOR  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR  CONSEQUENTIAL DAMAGES  (INCLUDING, BUT NOT  LIMITED TO,
PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS  OF USE,  DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER  IN CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING
NEGLIGENCE OR  OTHERWISE) ARISING IN  ANY WAY OUT  OF THE USE  OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This work was produced at Lawrence Livermore National Laboratory under
Contract  No.   DE-AC52-07NA27344 with  the  DOE.  Neither the  United
States Government  nor Lawrence  Livermore National Security,  LLC nor
any of  their employees,  makes any warranty,  express or  implied, or
assumes   any   liability   or   responsibility  for   the   accuracy,
completeness, or usefulness of any information, apparatus, product, or
process  disclosed, or  represents  that its  use  would not  infringe
privately-owned   rights.  Any  reference   herein  to   any  specific
commercial products,  process, or  services by trade  name, trademark,
manufacturer or otherwise does not necessarily constitute or imply its
endorsement,  recommendation,   or  favoring  by   the  United  States
Government or Lawrence Livermore National Security, LLC. The views and
opinions  of authors  expressed  herein do  not  necessarily state  or
reflect those  of the United  States Government or  Lawrence Livermore
National  Security, LLC,  and shall  not  be used  for advertising  or
product endorsement purposes.
*/
#define H5_USE_16_API
#include <math.h>

#include <hdf5.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

/* useful macro for comparing HDF5 versions */
#define HDF5_VERSION_GE(Maj,Min,Rel)  \
        (((H5_VERS_MAJOR==Maj) && (H5_VERS_MINOR==Min) && (H5_VERS_RELEASE>=Rel)) || \
         ((H5_VERS_MAJOR==Maj) && (H5_VERS_MINOR>Min)) || \
         (H5_VERS_MAJOR>Maj))

/* Simple program description 
   
   This program tests straightforward use of (serial) HDF5 to write (and read) datasets.

   Datasets are always written in a single H5Dwrite call and always read in single
   H5Dread call. No partial I/O is performed.

   Datasets are written at the leaves of a 1, 2 or 3-level deep group hierarchy.

   Datasets may be compressed (only zlib is currently used) and may be written
   int compact or contiguous mode.

   Running the program with no args, does a default run of 1000 datasets, all in
   the root directory. Each dataset is a single double. The program *always* prints
   its test parameters before running and emits progress at ~5% progress steps
   throughout the run. The best way to get a list of supported command-line options
   is simply to run the program with no args. But, here are some command-line examples...

       ./testhdf5 nd=50,10 dsize=250 contig=1

           creates 50 dirs, and 10 datasets of size 250 doubles in each dir using
           contiguous storage.

       ./testhdf5 nd=50,10 dsize=250 contig=1 doread=1

           will try to re-read the file. Note, you should re-read files created
           only with identical command-line args.
  
       ./testhdf5 nd=50,10 dsize=250 contig=1 doread=2

           will verify the read data

       ./testhdf5 nd=10,10,10,10 dsize=50000

           will generate a 4-level deep tree where each node is 10 dirs and datasets
           of size 50,000 doubles at the bottom.
*/

/* returns time in seconds */
double GetTime()
{
    static double t0 = -1;
    double t1;
    struct timeval tv1;

    if (t0<0)
    {
        struct timeval tv0;
        gettimeofday(&tv0, 0);
        t0 = (double)tv0.tv_sec*1e+6+(double)tv0.tv_usec;
        return 0;
    }

    gettimeofday(&tv1, 0);
    t1 = (double)tv1.tv_sec*1e+6+(double)tv1.tv_usec;

    return (t1-t0)/1e+6;
}

static void create_dataset(int idx, hid_t grp, int doread, int contig, int dsize,
    int zip, int noise, int *running_count, double *running_time)
{
    char dsname[32];
    static double *dbuf = 0, *rbuf = 0;
    int i, ndims = 1;
    hsize_t dims = dsize; 
    hid_t double_ds_id = 0;
    hid_t double_space_id;
    hid_t dcprops;
    double *bufp;
    double t0;

    if (idx < 0 && dbuf)
    {
        free(dbuf);
        if (rbuf) free(rbuf);
        return;
    }

    if (!dbuf)
    {
        if (doread)
            rbuf = (double *) malloc(dsize * sizeof(double));

        if (noise)
        {
            /* We wanna generate random #'s only once. We generate a 2x larger set
               and then on each iteration we randomly pick a starting point in the
               buf to write on each iteration */
            dbuf = (double *) malloc(2 * dsize * sizeof(double));
            srandom(0xDeadBeef);
            for (i = 0; i < 2 * dsize; i++)
                dbuf[i] = (double) (random()%noise) / ((random()%noise)+1) *
                          sin(2*M_PI*(random()%100000)/(random()%100000));
        }
        else
        {
            dbuf = (double *) malloc(dsize * sizeof(double));
        }
    }

    if (!noise)
    {
        for (i = 0; i < dsize; i++)
            dbuf[i] = (double) idx + i*0.00000001;
    }

    if (!doread)
    {
        double_space_id = H5Screate_simple(ndims, &dims, 0);

        dcprops = H5Pcreate(H5P_DATASET_CREATE);
        if (contig || dsize >= 8192)
            H5Pset_layout(dcprops, H5D_CONTIGUOUS);
        else
            H5Pset_layout(dcprops, H5D_COMPACT);

        if (zip)
        {
            H5Pset_layout(dcprops, H5D_CHUNKED);
            H5Pset_chunk(dcprops, ndims, &dims);
            H5Pset_shuffle(dcprops);
            H5Pset_deflate(dcprops, zip);
        }
    }

    snprintf(dsname, sizeof(dsname), "doubles_%08d", idx);

    if (doread)
    {
        if (H5Iis_valid(grp) > 0)
            double_ds_id = H5Dopen2(grp, dsname, H5P_DEFAULT);
        if (H5Iis_valid(double_ds_id) > 0)
        {
            t0 = GetTime();
            H5Dread(double_ds_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, rbuf);
            if (doread > 1)
            {
                if (memcmp(rbuf, dbuf, dsize*sizeof(double)) != 0)
                {
                    char dirname[512];
                    H5Iget_name(grp, dirname, sizeof(dirname));
                    printf("Verification failed on dataset \"%s\" in dir \"%s\"\n", dsname, dirname);
                }
            }
        }
        else if (doread > 1)
        {
            char dirname[512];
            snprintf(dirname, sizeof(dirname), "unknown");
            if (H5Iis_valid(grp) > 0)
                H5Iget_name(grp, dirname, sizeof(dirname));
            printf("Verification failed on dataset \"%s\" in dir \"%s\"\n", dsname, dirname);
        }
    }
    else
    {
        double_ds_id = H5Dcreate(grp, dsname, H5T_NATIVE_DOUBLE, double_space_id, dcprops);
        H5Sclose(double_space_id);
        H5Pclose(dcprops);
        t0 = GetTime();
        H5Dwrite(double_ds_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, noise?&dbuf[random()%dsize]:dbuf);
    }
    if (H5Iis_valid(double_ds_id) > 0)
        H5Dclose(double_ds_id);

    *running_time = *running_time + GetTime() - t0;
    *running_count = *running_count + 1;
}

static hid_t create_group(hid_t gid, int idx, int level, int doread, int estlink, int maxlink,
    char const *parent, char *newdir, int *running_count)
{
    hid_t gcprops, grp;
    char tmpDir[40];

    snprintf(tmpDir, sizeof(tmpDir), "level_%1d_%06d", level, idx);
    snprintf(newdir, level*sizeof(tmpDir), "%s/%s", parent, tmpDir);

    if (doread)
    {
        if (H5Iis_valid(gid) > 0)
        {
            H5E_BEGIN_TRY {
                grp = H5Gopen2(gid, tmpDir, H5P_DEFAULT);
            } H5E_END_TRY;
        }
    }
    else
    {
        gcprops = H5Pcreate(H5P_GROUP_CREATE);
        if (estlink)
            H5Pset_est_link_info(gcprops, maxlink, 16);
        grp = H5Gcreate2(gid, tmpDir, H5P_DEFAULT, gcprops, H5P_DEFAULT);
        H5Pclose(gcprops);
        H5Glink(grp, H5G_LINK_SOFT, parent, "..");
    }

    *running_count = *running_count + 1;
    return grp;
}

static hid_t file_create_props(int estlink, int maxlink)
{
    hid_t retval = H5Pcreate(H5P_FILE_CREATE);

    H5Pset_istore_k(retval, 1);

    if (estlink)
        H5Pset_est_link_info(retval, maxlink, 16);

    return retval;
}


static hid_t file_access_props(int compat, int cache)
{
    hid_t retval = H5Pcreate(H5P_FILE_ACCESS);

#if HDF5_VERSION_GE(1,6,0)
    if (!compat)
        H5Pset_libver_bounds(retval, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST);
#endif

    if (!cache)
        return retval;

#if !HDF5_VERSION_GE(1,6,4)
    H5Pset_cache(retval, cache, 1000, 10000, 0.5);
#elif HDF5_VERSION_GE(1,8,0)
    {
        H5AC_cache_config_t config;

        /* Acquire a default mdc config struct */
        config.version = H5AC__CURR_CACHE_CONFIG_VERSION;
        H5Pget_mdc_config(retval, &config);

        config.set_initial_size = (hbool_t) 1;
        config.initial_size = cache;
        config.min_size = cache;
        config.max_size = cache;
        config.incr_mode = H5C_incr__off;
        config.flash_incr_mode = H5C_flash_incr__off;
        config.decr_mode = H5C_decr__off;

#if 0
        config.incr_mode = H5C_incr__threshold;
        config.lower_hr_threshold = 0.9;
        config.increment = 2.0;
        config.decr_mode = H5C_decr__age_out;
        config.epoch_length = 1000;
        config.epochs_before_eviction = 2;
#endif
        H5Pset_mdc_config(retval, &config);
    }
#endif

    return retval;
}

static hid_t reopen_file(hid_t fid, int compat, int cache, char const *filename)
{
    hid_t faprops;
    H5Fclose(fid);
    faprops = file_access_props(compat, cache);
    fid = H5Fopen(filename, H5F_ACC_RDWR, faprops);
    H5Pclose(faprops);
    return fid;
}

static void progress(int n, int totn, hid_t fid, double dstm, double *minr, double *maxr)
{
    static int lastn = 0;
    static double lastt = 0;
    static double lastdstm = 0;

    if (n < 2) return;
    if (totn < 20) return;

    if (!(n % (totn/20)))
    {
        double t = GetTime();
        double rate;
        double dn, dt;

        dn = n-lastn;
        dt = t-lastt-(dstm-lastdstm);
        rate = dn/dt;

        printf("%3d%% complete, dt=%f secs, rate = %f objs/sec", 100*n/totn, dt, rate);
#if HDF5_VERSION_GE(1,6,4)
        printf(", number of open objects is %d\n", (int) H5Fget_obj_count(fid, H5F_OBJ_ALL));
#else
        printf("\n");
#endif

        if (*minr == 0) *minr = rate;
        if (*maxr == 0) *maxr = rate;
        if (rate < *minr) *minr = rate;
        if (rate > *maxr) *maxr = rate;

        lastn = n;
        lastt = t;
        lastdstm = dstm;
    }
}

#define PRINT_VAL(A,HELP)                      \
{                                              \
    char tmpstr[64];                           \
    int len = snprintf(tmpstr, sizeof(tmpstr), "%s=%d", #A, A); \
    printf("    %s=%d %*s\n",#A,A,60-len,#HELP); \
}

int main(int argc, char **argv)
{
    char const *filename = "test-hdf5-dirs.h5";
    int i, j, k, l, n=0, totn;
    int nd0 = 1000;
    int nd1 = 0;
    int nd2 = 0;
    int nd3 = 0;
    int compat = 0;
    int contig = 0;
    int dontae = 0;
    int estlink = 0;
    int freelim = 0;
    int gc = 0;
    int flush = 0;
    int closef = 0;
    int cache = 0;
    int dsize = 1;
    int maxlink, maxlink1, maxlink2;
    int dircnt = 0, dscnt = 0;
    int zip=0;
    int noise=0;
    int doread=0;
    double minrate = 0, maxrate = 0;
    double dstm = 0;
    hid_t fcprops, faprops;
    hid_t fid;

    for (i=1; i<argc; i++) {
        if (!strncmp(argv[i], "nd=", 3)) {
            char *p = argv[i], *q;
            nd0 = (int) strtol(p+3,&q,10);
            if (q && *q)
            {
                p = q + 1;
                nd1 = (int) strtol(p,&q,10);
            }
            if (q && *q)
            {
                p = q + 1;
                nd2 = (int) strtol(p,&q,10);
            }
            if (q && *q)
            {
                p = q + 1;
                nd3 = (int) strtol(p,&q,10);
            }
        } else if (!strncmp(argv[i], "compat=", 7)) {
            compat = (int) strtol(argv[i]+7,0,10);
        } else if (!strncmp(argv[i], "estlink=", 8)) {
            estlink = (int) strtol(argv[i]+8,0,10);
        } else if (!strncmp(argv[i], "gc=", 3)) {
            gc = (int) strtol(argv[i]+3,0,10);
        } else if (!strncmp(argv[i], "flush=", 6)) {
            flush = (int) strtol(argv[i]+6,0,10);
        } else if (!strncmp(argv[i], "closef=", 7)) {
            closef = (int) strtol(argv[i]+7,0,10);
        } else if (!strncmp(argv[i], "contig=", 7)) {
            contig = (int) strtol(argv[i]+7,0,10);
        } else if (!strncmp(argv[i], "dontae=", 7)) {
            dontae = (int) strtol(argv[i]+7,0,10);
        } else if (!strncmp(argv[i], "cache=", 6)) {
            cache = (int) strtol(argv[i]+6,0,10);
        } else if (!strncmp(argv[i], "freelim=", 8)) {
            freelim = (int) strtol(argv[i]+8,0,10);
        } else if (!strncmp(argv[i], "dsize=", 6)) {
            dsize = (int) strtol(argv[i]+6,0,10);
        } else if (!strncmp(argv[i], "zip=", 4)) {
            zip = (int) strtol(argv[i]+4,0,10);
        } else if (!strncmp(argv[i], "noise=", 6)) {
            noise = (int) strtol(argv[i]+6,0,10);
        } else if (!strncmp(argv[i], "doread=", 7)) {
            doread = (int) strtol(argv[i]+7,0,10);
        } else if (argv[i][0] != '\0') {
            fprintf(stderr, "%s: ignored argument `%s'\n", argv[0], argv[i]);
        }
    }

    totn = nd0;
    if (nd1) totn += nd0*nd1;
    if (nd2) totn += nd0*nd1*nd2;
    if (nd3) totn += nd0*nd1*nd2*nd3;

    maxlink = nd0>65535?65535:nd0;
    maxlink1 = nd1>65535?65535:nd1;
    maxlink2 = nd2>65535?65535:nd2;

    printf("Creates a 1, 2, or 3 level dir hierarchy with datasets at the bottom\n");
    printf("Test parameters...\n");
    PRINT_VAL(doread, do a read instead of a write test);
    PRINT_VAL(nd0, level 0 dir|dataset count);
    PRINT_VAL(nd1, level 1 dir|dataset count);
    PRINT_VAL(nd2, level 2 dir|dataset count);
    PRINT_VAL(nd3, level 3 dataset count);
    PRINT_VAL(dsize, dataset size in # doubles);
    PRINT_VAL(estlink, turn on link count estimation);
    PRINT_VAL(maxlink, computed value);
    PRINT_VAL(maxlink1, computed value);
    PRINT_VAL(maxlink2, computed value);
    PRINT_VAL(compat, turn on earliest libver compatability);
    PRINT_VAL(zip, turn on dataset compression);
    PRINT_VAL(noise, turn on dataset value randomizing);
    PRINT_VAL(gc, call garbabe collect after every <gc> objects);
    PRINT_VAL(flush, call flush after every <flush> objects);
    PRINT_VAL(closef, close and re-open file after every <closef> objects);
    PRINT_VAL(contig, turn on contiguous datasets);
    PRINT_VAL(dontae, do not atexit|close (helps with valgrind));
    PRINT_VAL(cache, set cache object (<=1.6.4) or byte (>1.6.4) count);
    PRINT_VAL(freelim, set free list limits to 1<<(<freelim>));

    if (dontae)
        H5dont_atexit();
    H5open();
    if (freelim)
    {
        int val = (1<<freelim);
        H5set_free_list_limits(val, val, val, val, val, val);
    }

    if (doread)
    {
        faprops = file_access_props(compat, cache);
        fid = H5Fopen(filename, H5F_ACC_RDONLY, faprops);
        H5Pclose(faprops);
    }
    else
    {
        faprops = file_access_props(compat, cache);
        fcprops = file_create_props(estlink, maxlink);
        fid = H5Fcreate(filename, H5F_ACC_TRUNC, fcprops, faprops);
        H5Pclose(fcprops);
        H5Pclose(faprops);
        H5Glink(fid, H5G_LINK_SOFT, "/", "..");
    }

    /* main loop */
    double t0 = GetTime();
    for (i = 0; i < nd0; i++, progress(n++,totn,fid,dstm,&minrate,&maxrate))
    {
        char dirName[40];
        hid_t grp1 = nd1 ? create_group(fid, i, 1, doread, estlink, maxlink, "/", dirName, &dircnt) : 0;

        for (j = 0; j < nd1; j++, progress(n++,totn,fid,dstm,&minrate,&maxrate))
        {
            char dirName1[80];
            hid_t grp2 = nd2 ? create_group(grp1, j, 2, doread, estlink, maxlink1, dirName, dirName1, &dircnt) : 0;

            for (k = 0; k < nd2; k++, progress(n++,totn,fid,dstm,&minrate,&maxrate))
            {
                char dirName2[120];
                hid_t grp3 = nd3 ? create_group(grp2, k, 3, doread, estlink, maxlink2, dirName1, dirName2, &dircnt) : 0;

                for (l = 0; l < nd3; l++, progress(n++,totn,fid,dstm,&minrate,&maxrate))
                {
                    create_dataset(l, grp3, doread, contig, dsize, zip, noise, &dscnt, &dstm);
                    if (flush && !(n%flush)) H5Fflush(fid, H5F_SCOPE_GLOBAL);
                    if (gc && !(n%gc)) H5garbage_collect();
                    if (closef && !(n%closef)) fid = reopen_file(fid, compat, cache, filename);
                }

                if (!nd3)
                    create_dataset(k, grp2, doread, contig, dsize, zip, noise, &dscnt, &dstm);

                if (H5Iis_valid(grp3) > 0) H5Gclose(grp3);
                if (flush && !(n%flush)) H5Fflush(fid, H5F_SCOPE_GLOBAL);
                if (gc && !(n%gc)) H5garbage_collect();
                if (closef && !(n%closef)) fid = reopen_file(fid, compat, cache, filename);
            }

            if (!nd2)
                create_dataset(j, grp1, doread, contig, dsize, zip, noise, &dscnt, &dstm);

            if (H5Iis_valid(grp2) > 0) H5Gclose(grp2);
            if (flush && !(n%flush)) H5Fflush(fid, H5F_SCOPE_GLOBAL);
            if (gc && !(n%gc)) H5garbage_collect();
            if (closef && !(n%closef)) fid = reopen_file(fid, compat, cache, filename);
        }

        if (!nd1)
            create_dataset(i, fid, doread, contig, dsize, zip, noise, &dscnt, &dstm);

        if (H5Iis_valid(grp1) > 0) H5Gclose(grp1);
        if (flush && !(n%flush)) H5Fflush(fid, H5F_SCOPE_GLOBAL);
        if (gc && !(n%gc)) H5garbage_collect();
        if (closef && !(n%closef)) fid = reopen_file(fid, compat, cache, filename);

    }
    create_dataset(-1, fid, doread, contig, dsize, zip, noise, &dscnt, &dstm); /* free static buffer in this func */
#if HDF5_VERSION_GE(1,6,4)
    printf("Upon close, number of open objects is %d\n", (int) H5Fget_obj_count(fid, H5F_OBJ_ALL));
#endif
    if (flush) H5Fflush(fid, H5F_SCOPE_GLOBAL);
    if (gc) H5garbage_collect();
    if (!dontae)
        H5Fclose(fid);

    {
    double t1 = GetTime();
    struct stat sbuf;
    stat(filename, &sbuf);
    {
    double totsecs = t1 - t0;
    double mdsecs = totsecs - dstm;
    unsigned long long all_bytes = sbuf.st_size;
    unsigned long long raw_bytes = dscnt*dsize*sizeof(double);
    unsigned long long other_bytes = all_bytes - raw_bytes;
    double raw_percent = 100 * (double) raw_bytes / (double) all_bytes;
    double other_percent = 100 - raw_percent; 

    printf("Total time = %8.4f seconds, dataset write time = %8.4f, other time = %8.4f (%4.2f %% of tot) seconds\n",
        totsecs, dstm, mdsecs, mdsecs/totsecs*100);
    printf("Total objects = %d: %d dirs, %d datasets (%4.2f %% of tot)\n",
        n, dircnt, dscnt, dscnt*100.0/n);
    printf("Object creation rate = %8.4f objs/sec, min=%8.4f, max=%8.4f, skew = %4.2f\n",
        n/mdsecs, minrate, maxrate, maxrate/minrate);
    if (zip && other_bytes > all_bytes)
    {
        printf("File size = %llu (compressed), overall zip ratio (w/overheads) = %4.2f : 1\n",
            (unsigned long long) sbuf.st_size, (double) raw_bytes / all_bytes);
    } 
    else
    {
        printf("File size = %llu, raw = %llu (%4.2f %%), other = %llu (%4.2f %%)\n",
            (unsigned long long) sbuf.st_size, raw_bytes, raw_percent, other_bytes, other_percent);
        printf("Average object overhead is ~%llu bytes\n", (all_bytes - raw_bytes) / n);
    }
    }
    }

}
