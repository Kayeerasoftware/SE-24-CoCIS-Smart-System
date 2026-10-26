#ifndef AUTH_SECURITY_H
#define AUTH_SECURITY_H

#include <stdbool.h>
#include <stdint.h>

// Initialization
void auth_security_init(void);

// Session Actions
bool auth_login(const char *username, const char *password);
void auth_logout(void);
bool auth_is_authenticated(void);
void auth_refresh_activity(void);

// User Info
const char *auth_get_logged_in_user(void);
uint32_t    auth_get_session_remaining_sec(void);

#endif // AUTH_SECURITY_H

