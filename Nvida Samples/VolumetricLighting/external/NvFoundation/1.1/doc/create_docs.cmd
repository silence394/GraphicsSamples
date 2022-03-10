set DOXYGEN_DIR=..\..\..\..\..\BuildTools\doxygen-win\bin
set HTMLHELP_DIR=..\..\..\..\..\BuildTools\HTMLHelpWorkshop

%DOXYGEN_DIR%\doxygen.exe docs.doxyfile
cd html
..\%HTMLHELP_DIR%\hhc.exe index.hhp
cd ..
