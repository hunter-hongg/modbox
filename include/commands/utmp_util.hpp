#ifndef UTMP_UTIL_HPP
#define UTMP_UTIL_HPP

#include <utmp.h>
#include <functional>

// Iterates every utmp record via setutent/getutent/endutent.
void for_each_utmp(const std::function<void(const struct utmp&)>& fn);

// Like for_each_utmp but only USER_PROCESS records with a non-empty user.
void for_each_utmp_user(const std::function<void(const struct utmp&)>& fn);

// Number of logged-in users (USER_PROCESS records with non-empty user).
int utmp_user_count();

#endif
