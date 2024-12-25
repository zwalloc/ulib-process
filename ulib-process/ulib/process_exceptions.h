#pragma once

#include <ulib/runtimeerror.h>

namespace ulib
{
    ULIB_RUNTIME_ERROR(process_error);
    class process_file_not_found_error : public process_error
    {
    public:
        process_file_not_found_error(ulib::string_view str) : process_error(str) {}
    };

    class process_invalid_flags_error : public process_error
    {
    public:
        process_invalid_flags_error(ulib::string_view str) : process_error(str) {}
    };

    class process_invalid_working_directory_error : public process_error
    {
    public:
        process_invalid_working_directory_error(ulib::string_view str) : process_error(str) {}
    };

    class process_internal_error : public process_error
    {
    public:
        process_internal_error(ulib::string_view str) : process_error(str) {}
    };
} // namespace ulib