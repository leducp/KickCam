#include <nanobind/nanobind.h>

#include "kickcam/version.h"

NB_MODULE(_native, m)
{
    m.attr("__version__") = KICKCAM_VERSION_STRING;
}
