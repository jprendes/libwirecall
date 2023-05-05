include(FetchContent)

FetchContent_Declare(wirepump
  GIT_REPOSITORY https://github.com/jprendes/libwirepump.git
  GIT_TAG        723f579f2be401d75127b36ebf9bc46c661e5f7a
)
FetchContent_MakeAvailable(wirepump)
FetchContent_GetProperties(wirepump)
