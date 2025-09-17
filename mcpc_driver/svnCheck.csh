#!/bin/csh
#
set OUTFILE = mcedev_ver.h
#
#set PROJECT_WORK_DIR = .
#
#set BLABLA = '#define MODVERSTRING "SVN revision: '
#set BLABLA2 = '"'
echo "" >  $OUTFILE
#  
#svn status -uvq $PROJECT_WORK_DIR > tmp
#cat tmp | grep -v "Status against revision" | gawk '{ORS = " "}{i=NF-3}{print $i}' > tmp2
#set svnNr = `cat tmp2 | gawk '{for (i=1;i<=NF;i=i+1) {if ($i == $1) {j=j+1}}}{if (j == NF) {print $1} else {print "-1"}}'`
#
#svn status -uq $PROJECT_WORK_DIR | grep -v "Status against revision" > tmp
#set modified = `grep "^[A-Za-z]" tmp`
#set star = '*'
#set outdated = `fgrep "$star" tmp`
#
#   set svnNr = `awk '/^Status against revision/{print $4}' < tmp`
#
#if( "$modified" == "" ) then
#  if( "$outdated" == "" ) then
#     echo "Your source tree has svn revision: $svnNr"
#     echo -n "${BLABLA}${svnNr}" > $OUTFILE
#     set ExitValue = 0
#     if ( "$svnNr" == "-1" ) then
#       echo "WARNING! Tree not updated!"
#       echo -n " not updated" >> $OUTFILE
#       set ExitValue = 3
#     endif
#  else
#     echo "Your source tree has svn revision: $svnNr"
#     echo -n "${BLABLA}${svnNr} outdated" > $OUTFILE
#     echo "WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!\nYour source tree contains the following outdated files:"
#     fgrep "$star" tmp | gawk '{print $NF}'
#     set ExitValue = 1
#  endif
#else
#   echo "Your source tree has svn revision: $svnNr"
#   echo "WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!\nYour source tree contains the following modified files:"
#   grep "^[A-Za-z]" tmp | gawk '{print $NF}'
#   echo -n "${BLABLA}${svnNr} modified" > $OUTFILE
#   set ExitValue = 2
#endif
#
#echo "${BLABLA2}" >> $OUTFILE
#
#\rm tmp
#\rm tmp2
#
#if ( "$1" != "release" ) then
#    set ExitValue = 0
#    \rm -f .SvnNr
#else
#    if ( "$ExitValue" == 0 ) then
#      echo "$svnNr" > .SvnNr
#      echo "SVN: $svnNr" 
#    endif
#endif
#
set ExitValue = 0
exit ${ExitValue}

