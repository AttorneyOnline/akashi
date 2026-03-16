#ifndef EXIT_CODE_H
#define EXIT_CODE_H

// Process exit codes for the server. Zero means a clean run.
enum class ExitCode : int
{
    Ok = 0,
    // The configuration folder is incomplete or config.ini is unusable.
    InvalidConfig = 1,
    // The configured bind address is not a usable IP address.
    InvalidBindAddress = 2,
    // The configured port could not be opened, for example because it is already in use.
    PortUnavailable = 3,
};

#endif // EXIT_CODE_H
