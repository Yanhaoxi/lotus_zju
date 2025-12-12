# Static Single Information (SSI) IR CMake configuration

add_library(CanarySSI STATIC
  SSIPass.cpp
  SSITransform.cpp
  SSIUtils.cpp
)
