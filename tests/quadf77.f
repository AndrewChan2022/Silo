C***********************************************************************
C Copyright (C) 1994-2016 Lawrence Livermore National Security, LLC.
C LLNL-CODE-425250.
C All rights reserved.
C 
C This file is part of Silo. For details, see silo.llnl.gov.
C 
C Redistribution and use in source and binary forms, with or without
C modification, are permitted provided that the following conditions
C are met:
C 
C    * Redistributions of source code must retain the above copyright
C      notice, this list of conditions and the disclaimer below.
C    * Redistributions in binary form must reproduce the above copyright
C      notice, this list of conditions and the disclaimer (as noted
C      below) in the documentation and/or other materials provided with
C      the distribution.
C    * Neither the name of the LLNS/LLNL nor the names of its
C      contributors may be used to endorse or promote products derived
C      from this software without specific prior written permission.
C 
C THIS SOFTWARE  IS PROVIDED BY  THE COPYRIGHT HOLDERS  AND CONTRIBUTORS
C "AS  IS" AND  ANY EXPRESS  OR IMPLIED  WARRANTIES, INCLUDING,  BUT NOT
C LIMITED TO, THE IMPLIED  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
C A  PARTICULAR  PURPOSE ARE  DISCLAIMED.  IN  NO  EVENT SHALL  LAWRENCE
C LIVERMORE  NATIONAL SECURITY, LLC,  THE U.S.  DEPARTMENT OF  ENERGY OR
C CONTRIBUTORS BE LIABLE FOR  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
C EXEMPLARY, OR  CONSEQUENTIAL DAMAGES  (INCLUDING, BUT NOT  LIMITED TO,
C PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS  OF USE,  DATA, OR
C PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
C LIABILITY, WHETHER  IN CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING
C NEGLIGENCE OR  OTHERWISE) ARISING IN  ANY WAY OUT  OF THE USE  OF THIS
C SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
C 
C This work was produced at Lawrence Livermore National Laboratory under
C Contract No.  DE-AC52-07NA27344 with the DOE.
C 
C Neither the  United States Government nor  Lawrence Livermore National
C Security, LLC nor any of  their employees, makes any warranty, express
C or  implied,  or  assumes  any  liability or  responsibility  for  the
C accuracy, completeness,  or usefulness of  any information, apparatus,
C product, or  process disclosed, or  represents that its use  would not
C infringe privately-owned rights.
C 
C Any reference herein to  any specific commercial products, process, or
C services by trade name,  trademark, manufacturer or otherwise does not
C necessarily  constitute or imply  its endorsement,  recommendation, or
C favoring  by  the  United  States  Government  or  Lawrence  Livermore
C National Security,  LLC. The views  and opinions of  authors expressed
C herein do not necessarily state  or reflect those of the United States
C Government or Lawrence Livermore National Security, LLC, and shall not
C be used for advertising or product endorsement purposes.
C***********************************************************************

C...............................................................................
C   Program
C
C       testquadf77.f
C
C   Purpose
C
C       Test program illustrating use of SILO from Fortran for
C       writing quad mesh and variable data.
C
C   Programmer
C
C       Jeff Long, NSSD/B, 11/24/92
C
C   Notes
C
C       For more info on calling sequences, see the "SILO User's Manual".
C...............................................................................


      program main

      implicit none
      include "silo.inc"

      integer  buildquad
      integer  dbid, meshid, err, driver, nargs, anint
      integer  iarr(20)
      real*8   darr(20)
      character*8 cname
      character*256 cloption

      driver = DB_PDB
      nargs = iargc()
      call getarg(1, cloption)
      if (cloption .eq. "DB_HDF5") then
          driver = DB_HDF5
      end if

      err = dbset2dstrlen(1024)

C...Create file.
      err = dbcreate("quadf77.silo", 12, DB_CLOBBER, DB_LOCAL,
     .               "file info", 9, driver, dbid)

      meshid = buildquad (dbid, "quad", 4)
      meshid = buildquad (dbid, "quad2", 5)

      call testdb (dbid)

      err = dbclose(dbid)

      print *,'Created file: quadf77.silo'




C...Read stuff.
      err = dbopen ("quadf77.silo", 12, driver, DB_READ, dbid)

      err = dbinqlen(dbid, "quad_coord0", 11, anint)
      if (err .ne. 0) print *, 'Unable to read quad_coord0 length'
      if (anint .ne. 4) print *, 'Error reading quad_coord0 length'

      err = dbinqdtyp(dbid, "quad_coord0", 11, anint)
      if (err .ne. 0) print *, 'Unable to read quad_coord0 data type'
      if (anint .ne. DB_FLOAT)
     .    print *, 'Error reading quad_coord0 data type'

      err = dbrdvar (dbid, "hcom2", 5, iarr)
      if (err .ne. 0) print *, 'Error reading hcom2'

      err = dbrdvar (dbid, "hcom3", 5, darr)
      if (err .ne. 0) print *, 'Error reading hcom3'

      err = dbrdvar (dbid, "namebase", 8, cname)
      if (err .ne. 0) print *, 'Error reading namebase'

C     err = dbrdvar (dbid, "junk", 4, cname)
C     if (err .ne. 0) print *, 'Error reading junk'



      stop
      end

      integer function buildquad (dbid, name, lname)
C----------------------------------------------------------------------
C  Routine                                                    buildquad
C
C  Purpose
C
C       Build quad-mesh, quad-var and return the mesh ID.
C
C  Modifications:
C    Kathleen Bonnell, Wed Sep 2 16:12:15 PDT 20099
C    Changed 'character*8 name' to 'character*(*) name' to remove 
C    'Character length argument mismatch' compiler error.
C
C----------------------------------------------------------------------

      integer  dbid
      character*(*) name
      integer lname

      parameter  (NX     = 4)
      parameter  (NY     = 3)
      parameter  (NZONES = 6)
      parameter  (NNODES = 12)

      include 'silo.inc'

      integer    i, meshid, varid
      integer    tcycle, mixlen, optlistid
      integer    dims(3), ndims
      real       x(NX), y(NY), d(NZONES), f(2*NZONES)
      real       ttime
      double precision dttime
      character*1024 vnames(2)
      integer lvnames(2)


C...Initializations.

      data  x/1.,2.,3.,4./
      data  y/1.,2.,3./
      data  d/1.,2.,3.,4.,5.,6./
      data  f/1.0,1.1,1.2,1.3,1.4,1.5,2.0,2.1,2.2,2.3,2.4,2.5/

      ttime     = 2.345
      dttime    = 2.345
      tcycle    = 200



C...Create an option list containing time and cycle information. Note
C...that the function names are different depending on the data type
C...of the option value. The variable 'optlistid' is used as a handle
C...to this option list in future function invocations.

      ierr = dbmkoptlist (5, optlistid)
      ierr = dbaddropt   (optlistid, DBOPT_TIME, ttime)    ! real
      ierr = dbaddiopt   (optlistid, DBOPT_CYCLE, tcycle)  ! integer
      ierr = dbadddopt   (optlistid, DBOPT_DTIME, dttime)  ! double

C...The following option would change the major order for multi-
C...dimensional arrays from row-major (default) to column-major.
C...If you have arrays defined like:  real  x(300,200) you might
C...need this.

C     ierr = dbaddiopt   (optlistid, DBOPT_MAJORORDER, DB_COLMAJOR)


C...Write simple 2-D quad mesh. Dims defines number of NODES. The
C...parameter 'meshid' is returned and should be used on future
C...writes of quad variables.

      ndims     = 2
      dims(1)   = NX
      dims(2)   = NY


      err = dbputqm(dbid, name, lname,
     .              "X", 1, "Y", 1, DB_F77NULLSTRING, 0,
     .              x, y, -1, dims, ndims, DB_FLOAT, DB_COLLINEAR,
     .              optlistid, meshid)


C...Write quad variable. Dims defines number of ZONES (since zone-centered).
C...Possible values for centering are: DB_ZONECENT, DB_NODECENT, DB_NOTCENT.

      ndims   = 2
      dims(1) = NX - 1
      dims(2) = NY - 1

C...Hack to make sure we don't wind up writing same variable name twice to
C...two different meshes
      if (lname .eq. 4) err = dbputqv1 (dbid, "d", 1, name,
     .                lname, d, dims, ndims,
     .                DB_F77NULL, 0, DB_FLOAT, DB_ZONECENT,
     .                optlistid, varid)
      if (lname .eq. 5) err = dbputqv1 (dbid, "e", 1, name,
     .                lname, d, dims, ndims,
     .                DB_F77NULL, 0, DB_FLOAT, DB_ZONECENT,
     .                optlistid, varid)
      vnames(1) = "FooBar"
      lvnames(1) = 6
      vnames(2) = "gorfo"
      lvnames(2) = 5
      err = dbputqv(dbid, "f", 1, name, lname, 2, vnames, lvnames,
     .          f, dims, ndims, DB_F77NULL, 0, DB_FLOAT, DB_ZONECENT,
     .          optlistid, varid)

      buildquad = meshid

      return
      end

C************************************************************
C************************************************************
      subroutine testdb (dbid)
      integer dbid

      include 'silo.inc'

      character*8 namebase
      integer  ilen, ihfirst(10)
      real*8   rhfirst(15)

      do i = 1 , 10
         ihfirst(i) = i
      enddo
      do i = 1 , 15
         rhfirst(i) = dble(i)
      enddo
      namebase = 'abcdefgh'
      ilen     = 10


      ierr = dbwrite ( dbid, "namebase", 8, namebase, 8, 1, DB_CHAR )
      ierr = dbwrite ( dbid, "ilen",     4, ilen,     1, 1, DB_INT )

C                       write integer hydro common to dump
      ialen = 10
      ierr = dbwrite ( dbid, "hcom2", 5, ihfirst, ialen, 1, DB_INT )

C                       write real hydro common to dump
      ialen = 15
      ierr = dbwrite ( dbid, "hcom3", 5, rhfirst, ialen, 1,DB_DOUBLE)


      return
      end
