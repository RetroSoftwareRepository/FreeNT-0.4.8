

#define SVC_(name, argcount) "Nt" #name,

const char * DbgSyscallNames[] =
{
#include <sysfuncs.h>
};

const unsigned int DbgNumberOfSyscalls = sizeof(DbgSyscallNames) / sizeof(DbgSyscallNames[0]);

