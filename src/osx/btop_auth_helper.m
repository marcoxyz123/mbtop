/* btop++ Authentication Helper

   This helper uses macOS Authorization Services to execute privileged commands.
   Shows "btop_auth_helper" (or "btop" if renamed) in the macOS password dialog.

   Usage: btop_auth_helper <command> [args...]
   Example: btop_auth_helper /usr/sbin/sysctl -w iogpu.wired_limit_mb=16384

   Exit codes:
     0 - Success
     1 - Authorization failed
     2 - User cancelled
     3 - Invalid arguments
*/

#import <Foundation/Foundation.h>
#import <Security/Authorization.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    @autoreleasepool {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
            return 3;
        }

        // Create authorization reference
        AuthorizationRef authRef = NULL;
        OSStatus status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                                              kAuthorizationFlagDefaults, &authRef);

        if (status != errAuthorizationSuccess) {
            fprintf(stderr, "Failed to create authorization: %d\n", (int)status);
            return 1;
        }

        // The command to execute
        const char *tool = argv[1];

        // Build arguments array (skip program name and command)
        char **args = (argc > 2) ? &argv[2] : NULL;

        // Execute with privileges - this shows the auth dialog
        FILE *pipe = NULL;

        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        status = AuthorizationExecuteWithPrivileges(authRef, tool,
                                                    kAuthorizationFlagDefaults,
                                                    args, &pipe);
        #pragma clang diagnostic pop

        // Read and discard output
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                // Optionally print output for debugging
            }
            fclose(pipe);
        }

        AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);

        if (status == errAuthorizationSuccess) {
            return 0;
        } else if (status == errAuthorizationCanceled) {
            return 2;
        } else {
            fprintf(stderr, "Authorization failed: %d\n", (int)status);
            return 1;
        }
    }
}
