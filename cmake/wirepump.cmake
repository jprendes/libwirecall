include(FetchContent)

FetchContent_Declare(wirepump
  GIT_REPOSITORY https://github.com/jprendes/libwirepump.git
  GIT_TAG        724bcd697c1d655747cf77e654186b80b2d8bbbc
)
FetchContent_MakeAvailable(wirepump)
FetchContent_GetProperties(wirepump)
