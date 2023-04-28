include(FetchContent)

FetchContent_Declare(wirepump
  GIT_REPOSITORY https://github.com/jprendes/libwirepump.git
  GIT_TAG        dba5eb34a975b30d0c16147884fa598a7817d3a8
)
FetchContent_MakeAvailable(wirepump)
FetchContent_GetProperties(wirepump)
