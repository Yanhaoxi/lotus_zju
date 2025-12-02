# try to find MPFI and MPFR
# if successful, set the following variables
#  MPFR_FOUND
#  MPFI_FOUND
#  MPFR_INCLUDEDIR
#  MPFR_LIBRARIES
#  MPFI_INCLUDEDIR
#  MPFI_LIBRARIES

find_path(MPFR_c_HEADER mpfr.h
        ${MPFR_INCLUDEDIR} # this to allow the user to provide a custom
        # location for MPFR
        /usr/include /usr/local/include)
find_path(MPFI_c_HEADER mpfi.h
        ${MPFI_INCLUDEDIR}
        /usr/include /usr/local/include)
find_path(glpk_c_HEADER glpk.h
        ${glpk_INCLUDEDIR}
        /usr/include /usr/local/include)

find_library(MPFR_c_LIBRARY
        NAMES mpfr
        PATHS ${MPFR_LIBRARIES} # this to allow the user to provide a
        # custom location for GMP
        /usr/lib /usr/local/lib)

find_library(MPFI_c_LIBRARY
        NAMES mpfi
        PATHS ${MPFI_LIBRARIES}
        /usr/lib /usr/local/lib)

find_library(glpk_c_LIBRARY
        NAMES glpk
        PATHS ${glpk_LIBRARIES}
        /usr/lib /usr/local/lib)

if(MPFR_c_HEADER AND MPFR_c_LIBRARY)
    set(MPFR_FOUND "YES")
    set(MPFR_INCLUDEDIR ${MPFR_c_HEADER})
    set(MPFR_LIBRARIES ${MPFR_c_LIBRARY})
endif(MPFR_c_HEADER AND MPFR_c_LIBRARY)

if(MPFI_c_HEADER AND MPFI_c_LIBRARY)
    set(MPFI_FOUND "YES")
    set(MPFI_INCLUDEDIR ${MPFI_c_HEADER})
    set(MPFI_LIBRARIES ${MPFI_c_LIBRARY})
endif(MPFI_c_HEADER AND MPFI_c_LIBRARY)

if(glpk_c_HEADER AND glpk_c_LIBRARY)
    set(glpk_FOUND "YES")
    set(glpk_INCLUDEDIR ${glpk_c_HEADER})
    set(glpk_LIBRARIES ${glpk_c_LIBRARY})
endif(glpk_c_HEADER AND glpk_c_LIBRARY)

mark_as_advanced(
       MPFI_c_HEADER
        MPFI_c_LIBRARY
        MPFR_c_HEADER
        MPFR_c_LIBRARY
        glpk_c_HEADER
        glpk_c_LIBRARY
)