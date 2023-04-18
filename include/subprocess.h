#ifndef __SUBPROCESS_H__
#define __SUBPROCESS_H__

#include <stdio.h>
#include <string.h>

#define subprocess_pure __attribute__((pure))
#define subprocess_weak __attribute__((weak))

struct subprocess;

enum subprocess_option_e
{
    // stdout and stderr are the same FILE.
    subprocess_option_combined_stdout_stderr = 0x1,

    // The child process should inherit the environment variables of the parent.
    subprocess_option_inherit_environment = 0x2,

    // Enable asynchronous reading of stdout/stderr before it has completed.
    subprocess_option_enable_async = 0x4,

    // Enable the child process to be spawned with no window visible if supported
    // by the platform.
    subprocess_option_no_window = 0x8,

    // Search for program names in the PATH variable. Always enabled on Windows.
    subprocess_option_search_user_path = 0x10
};

#if defined(__cplusplus)
extern "C"
{
#endif

/// @brief Create a process.
/// @param command_line An array of strings for the command line to execute for
/// this process. The last element must be NULL to signify the end of the array.
/// The memory backing this parameter only needs to persist until this function
/// returns.
/// @param options A bit field of subprocess_option_e's to pass.
/// @param out_process The newly created process.
/// @return On success zero is returned.
subprocess_weak int subprocess_create(const char *const command_line[],
                                        int options,
                                        struct subprocess *const out_process);

/// @brief Create a process (extended create).
/// @param command_line An array of strings for the command line to execute for
/// this process. The last element must be NULL to signify the end of the array.
/// The memory backing this parameter only needs to persist until this function
/// returns.
/// @param options A bit field of subprocess_option_e's to pass.
/// @param environment An optional array of strings for the environment to use
/// for a child process (each element of the form FOO=BAR). The last element
/// must be NULL to signify the end of the array.
/// @param out_process The newly created process.
/// @return On success zero is returned.
///
/// If `options` contains `subprocess_option_inherit_environment`, then
/// `environment` must be NULL.
subprocess_weak int
subprocess_create_ex(const char *const command_line[], int options,
                        const char *const environment[],
                        struct subprocess *const out_process);

/// @brief Get the standard input file for a process.
/// @param process The process to query.
/// @return The file for standard input of the process.
///
/// The file returned can be written to by the parent process to feed data to
/// the standard input of the process.
subprocess_pure subprocess_weak FILE *
subprocess_stdin(const struct subprocess *const process);

/// @brief Get the standard output file for a process.
/// @param process The process to query.
/// @return The file for standard output of the process.
///
/// The file returned can be read from by the parent process to read data from
/// the standard output of the child process.
subprocess_pure subprocess_weak FILE *
subprocess_stdout(const struct subprocess *const process);

/// @brief Get the standard error file for a process.
/// @param process The process to query.
/// @return The file for standard error of the process.
///
/// The file returned can be read from by the parent process to read data from
/// the standard error of the child process.
///
/// If the process was created with the subprocess_option_combined_stdout_stderr
/// option bit set, this function will return NULL, and the subprocess_stdout
/// function should be used for both the standard output and error combined.
subprocess_pure subprocess_weak FILE *
subprocess_stderr(const struct subprocess *const process);

/// @brief Wait for a process to finish execution.
/// @param process The process to wait for.
/// @param out_return_code The return code of the returned process (can be
/// NULL).
/// @return On success zero is returned.
///
/// Joining a process will close the stdin pipe to the process.
subprocess_weak int subprocess_join(struct subprocess *const process,
                                    int *const out_return_code);

/// @brief Destroy a previously created process.
/// @param process The process to destroy.
/// @return On success zero is returned.
///
/// If the process to be destroyed had not finished execution, it may out live
/// the parent process.
subprocess_weak int subprocess_destroy(struct subprocess *const process);

/// @brief Terminate a previously created process.
/// @param process The process to terminate.
/// @return On success zero is returned.
///
/// If the process to be destroyed had not finished execution, it will be
/// terminated (i.e killed).
subprocess_weak int subprocess_terminate(struct subprocess *const process);

/// @brief Read the standard output from the child process.
/// @param process The process to read from.
/// @param buffer The buffer to read into.
/// @param size The maximum number of bytes to read.
/// @return The number of bytes actually read into buffer. Can only be 0 if the
/// process has complete.
///
/// The only safe way to read from the standard output of a process during it's
/// execution is to use the `subprocess_option_enable_async` option in
/// conjuction with this method.
subprocess_weak unsigned
subprocess_read_stdout(struct subprocess *const process, char *const buffer,
                        unsigned size);

/// @brief Read the standard error from the child process.
/// @param process The process to read from.
/// @param buffer The buffer to read into.
/// @param size The maximum number of bytes to read.
/// @return The number of bytes actually read into buffer. Can only be 0 if the
/// process has complete.
///
/// The only safe way to read from the standard error of a process during it's
/// execution is to use the `subprocess_option_enable_async` option in
/// conjuction with this method.
subprocess_weak unsigned
subprocess_read_stderr(struct subprocess *const process, char *const buffer,
                        unsigned size);

/// @brief Returns if the subprocess is currently still alive and executing.
/// @param process The process to check.
/// @return If the process is still alive non-zero is returned.
subprocess_weak int subprocess_alive(struct subprocess *const process);

#if defined(__cplusplus)
#define SUBPROCESS_CAST(type, x) static_cast<type>(x)
#define SUBPROCESS_PTR_CAST(type, x) reinterpret_cast<type>(x)
#define SUBPROCESS_CONST_CAST(type, x) const_cast<type>(x)
#define SUBPROCESS_NULL NULL
#else
#define SUBPROCESS_CAST(type, x) ((type)(x))
#define SUBPROCESS_PTR_CAST(type, x) ((type)(x))
#define SUBPROCESS_CONST_CAST(type, x) ((type)(x))
#define SUBPROCESS_NULL 0
#endif

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct subprocess
{
    FILE *stdin_file;
    FILE *stdout_file;
    FILE *stderr_file;
    pid_t child;
    int return_status;
    size_t alive;
};

int subprocess_create(const char *const commandLine[], int options,
                        struct subprocess *const out_process)
{
    return subprocess_create_ex(commandLine, options, SUBPROCESS_NULL,
                                out_process);
}

int subprocess_create_ex(const char *const commandLine[], int options,
                            const char *const environment[],
                            struct subprocess *const out_process)
{
    int stdinfd[2];
    int stdoutfd[2];
    int stderrfd[2];
    pid_t child;
    extern char **environ;
    char *const empty_environment[1] = {SUBPROCESS_NULL};

    if (subprocess_option_inherit_environment ==
        (options & subprocess_option_inherit_environment)) {
        if (SUBPROCESS_NULL != environment) {
            return -1;
        }
    }

    if (0 != pipe(stdinfd)) {
        return -1;
    }

    if (0 != pipe(stdoutfd)) {
        return -1;
    }

    if (subprocess_option_combined_stdout_stderr !=
        (options & subprocess_option_combined_stdout_stderr)) {
        if (0 != pipe(stderrfd)) {
            return -1;
        }
    }

    child = fork();

    if (-1 == child) {
        return -1;
    }

    if (0 == child) {
        // Close the stdin write end
        close(stdinfd[1]);
        // Map the read end to stdin
        dup2(stdinfd[0], STDIN_FILENO);

        // Close the stdout read end
        close(stdoutfd[0]);
        // Map the write end to stdout
        dup2(stdoutfd[1], STDOUT_FILENO);

        if (subprocess_option_combined_stdout_stderr ==
            (options & subprocess_option_combined_stdout_stderr)) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        }
        else {
            // Close the stderr read end
            close(stderrfd[0]);
            // Map the write end to stdout
            dup2(stderrfd[1], STDERR_FILENO);
        }

        if (environment) {
            environ = (char **)environment;
        }
        else if (subprocess_option_inherit_environment !=
                    (options & subprocess_option_inherit_environment)) {
            environ = (char **)empty_environment;
        }

        if (subprocess_option_search_user_path == (options & subprocess_option_search_user_path)) {
            _Exit(execvp(commandLine[0], (char *const *)commandLine));
        }
        else {
            _Exit(execv(commandLine[0], (char *const *)commandLine));
        }
    }
    else {
        // Close the stdin read end
        close(stdinfd[0]);
        // Store the stdin write end
        out_process->stdin_file = fdopen(stdinfd[1], "wb");

        // Close the stdout write end
        close(stdoutfd[1]);
        // Store the stdout read end
        out_process->stdout_file = fdopen(stdoutfd[0], "rb");

        if (subprocess_option_combined_stdout_stderr ==
            (options & subprocess_option_combined_stdout_stderr)) {
            out_process->stderr_file = out_process->stdout_file;
        }
        else {
            // Close the stderr write end
            close(stderrfd[1]);
            // Store the stderr read end
            out_process->stderr_file = fdopen(stderrfd[0], "rb");
        }

        // Store the child's pid
        out_process->child = child;

        out_process->alive = 1;

        return 0;
    }
}

FILE *subprocess_stdin(const struct subprocess *const process)
{
    return process->stdin_file;
}

FILE *subprocess_stdout(const struct subprocess *const process)
{
    return process->stdout_file;
}

FILE *subprocess_stderr(const struct subprocess *const process)
{
    if (process->stdout_file != process->stderr_file) {
        return process->stderr_file;
    }
    else {
        return SUBPROCESS_NULL;
    }
}

int subprocess_join(struct subprocess *const process,
                    int *const out_return_code)
{
    int status;

    if (process->stdin_file) {
        fclose(process->stdin_file);
        process->stdin_file = SUBPROCESS_NULL;
    }

    if (process->child) {
        if (process->child != waitpid(process->child, &status, 0)) {
            return -1;
        }

        process->child = 0;

        if (WIFEXITED(status)) {
            process->return_status = WEXITSTATUS(status);
        }
        else {
            process->return_status = EXIT_FAILURE;
        }

        process->alive = 0;
    }

    if (out_return_code) {
        *out_return_code = process->return_status;
    }

    return 0;
}

int subprocess_destroy(struct subprocess *const process)
{
    if (process->stdin_file) {
        fclose(process->stdin_file);
        process->stdin_file = SUBPROCESS_NULL;
    }

    if (process->stdout_file) {
        fclose(process->stdout_file);

        if (process->stdout_file != process->stderr_file) {
            fclose(process->stderr_file);
        }

        process->stdout_file = SUBPROCESS_NULL;
        process->stderr_file = SUBPROCESS_NULL;
    }
    return 0;
}

int subprocess_terminate(struct subprocess *const process)
{
    int result;
    result = kill(process->child, 9);
    return result;
}

unsigned subprocess_read_stdout(struct subprocess *const process,
                                char *const buffer, unsigned size)
{
    const int fd = fileno(process->stdout_file);
    const ssize_t bytes_read = read(fd, buffer, size);

    if (bytes_read < 0) {
        return 0;
    }

    return SUBPROCESS_CAST(unsigned, bytes_read);
}

unsigned subprocess_read_stderr(struct subprocess *const process,
                                char *const buffer, unsigned size)
{
    const int fd = fileno(process->stderr_file);
    const ssize_t bytes_read = read(fd, buffer, size);

    if (bytes_read < 0) {
        return 0;
    }

    return SUBPROCESS_CAST(unsigned, bytes_read);
}

int subprocess_alive(struct subprocess *const process)
{
    int is_alive = SUBPROCESS_CAST(int, process->alive);

    if (!is_alive) {
        return 0;
    }

    int status;
    is_alive = 0 == waitpid(process->child, &status, WNOHANG);

    // If the process was successfully waited on we need to cleanup now.
    if (!is_alive) {
        if (WIFEXITED(status)) {
            process->return_status = WEXITSTATUS(status);
        }
        else {
            process->return_status = EXIT_FAILURE;
        }

        // Since we've already successfully waited on the process, we need to wipe
        // the child now.
        process->child = 0;

        if (subprocess_join(process, SUBPROCESS_NULL)) {
            return -1;
        }
    }

    if (!is_alive) {
        process->alive = 0;
    }

    return is_alive;
}

#if defined(__cplusplus)
} // extern "C"
#endif

#endif /* __SUBPROCESS_H__ */
