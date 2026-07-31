#include "commands/utmp_util.hpp"

void for_each_utmp(const std::function<void(const struct utmp&)>& fn) {
    setutent();
    struct utmp* u;
    while ((u = getutent()) != nullptr) {
        fn(*u);
    }
    endutent();
}

void for_each_utmp_user(const std::function<void(const struct utmp&)>& fn) {
    for_each_utmp([&fn](const struct utmp& u) {
        if (u.ut_type == USER_PROCESS && u.ut_user[0] != '\0') {
            fn(u);
        }
    });
}

int utmp_user_count() {
    int count = 0;
    for_each_utmp_user([&count](const struct utmp&) { count++; });
    return count;
}
