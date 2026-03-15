#include <cmocka.h>

#include "../mem.h"
#include "../fileconf.h"

#define TestParser(PARSER, DESC, LINE, EXPECTED_COMMAND) \
{ \
    cf_t cf; \
    char *input = strdup2(LINE); \
    PARSER(input, &cf); \
    assert_string_equal(cf.cf_line_base->cl_shell, EXPECTED_COMMAND); \
    Free_safe(input); \
}

#define TestArysParser(D, L, E) TestParser(read_arys, D, L, E)
#define TestPeriodParser(D, L, E) TestParser(read_period, D, L, E)
#define TestFreqParser(D, L, E) TestParser(read_freq, D, L, E)
#define TestShortcutParser(D, L, E) TestParser(read_shortcut, D, L, E)

static void
test_read_arys_shell_parser(void **state)
{
    (void)state; /* unused */
    TestArysParser("example 1",
                   "& 05,35 12-14 * * * mycommand -u me -o file ",
                   "mycommand -u me -o file");
    TestArysParser("quoted command",
                   "* * * * * \"/root/my script\"",
                   "\"/root/my script\"");
    TestArysParser("Hourly Git Maintenance",
                   "32 1-23 * * * \"/usr/libexec/git-core/git\" --exec-path=\"/usr/libexec/git-core\" -c credential.interactive=false -c core.askPass=true  for-each-repo --keep-going --config=maintenance.repo maintenance run --schedule=hourly",
                   "\"/usr/libexec/git-core/git\" --exec-path=\"/usr/libexec/git-core\" -c credential.interactive=false -c core.askPass=true  for-each-repo --keep-going --config=maintenance.repo maintenance run --schedule=hourly");
    TestArysParser("Daily Git Maintenance",
                   "32 0 * * 1-6 \"/usr/libexec/git-core/git\" --exec-path=\"/usr/libexec/git-core\" -c credential.interactive=false -c core.askPass=true  for-each-repo --keep-going --config=maintenance.repo maintenance run --schedule=daily",
                   "\"/usr/libexec/git-core/git\" --exec-path=\"/usr/libexec/git-core\" -c credential.interactive=false -c core.askPass=true  for-each-repo --keep-going --config=maintenance.repo maintenance run --schedule=daily");
    TestArysParser("Weekly Git Maintenance",
                   "32 0 * * 0 \"/usr/libexec/git-core/git\" --exec-path=\"/usr/libexec/git-core\" -c credential.interactive=false -c core.askPass=true  for-each-repo --keep-going --config=maintenance.repo maintenance run --schedule=weekly",
                   "\"/usr/libexec/git-core/git\" --exec-path=\"/usr/libexec/git-core\" -c credential.interactive=false -c core.askPass=true  for-each-repo --keep-going --config=maintenance.repo maintenance run --schedule=weekly");
}

static void
test_read_freq_shell_parser(void **state)
{
    (void)state; /* unused */
    TestFreqParser("example 1",
                   "@ 30 getmails -all",
                   "getmails -all");
    TestFreqParser("example 2",
                   "@mailto(root),forcemail 2d /etc/security/msec/cron-sh/security.sh",
                   "/etc/security/msec/cron-sh/security.sh");
    TestFreqParser("quoted command",
                   "@ 12h02 \"/root/my script\"",
                   "\"/root/my script\"");
    TestFreqParser("internal quotes",
                   "@ 3d echo \"hi\"",
                   "echo \"hi\"");
    TestFreqParser("blanks",
                   "@ 3w2d5h1 true  ",
                   "true");
}

static void
test_read_period_shell_parser(void **state)
{
    (void)state; /* unused */
    TestPeriodParser("example 1",
                     "%nightly,mail(no) * 21-23,3-5 echo \"a nigthly entry\"",
                     "echo \"a nigthly entry\"");
    TestPeriodParser("example 2",
                     "%hours * 0-22 * * * echo \"Ok.\"",
                     "echo \"Ok.\"");
    TestPeriodParser("quoted command",
                     "%hourly 31 \"/root/my script\"",
                     "\"/root/my script\"");
    TestPeriodParser("internal quotes",
                     "%middaily 21 7-10 echo \"hi\"",
                     "echo \"hi\"");
    TestPeriodParser("blanks",
                     "%monthly 59 4 12 true  ",
                     "true");
}

static void
test_read_shortcut_shell_parser(void **state)
{
    (void)state; /* unused */
    TestShortcutParser("example 1",
                       "@hourly check_laptop_logs.sh",
                       "check_laptop_logs.sh");
    TestShortcutParser("example 2",
                       "@daily check_web_server.sh",
                       "check_web_server.sh");
    TestShortcutParser("example 3",
                       "@daily check_file_server.sh",
                       "check_file_server.sh");
    TestShortcutParser("example 4",
                       "@monthly compress_home_made_app_log_files.sh",
                       "compress_home_made_app_log_files.sh");
    TestShortcutParser("quoted command",
                       "@weekly \"/root/my script\"",
                       "\"/root/my script\"");
    TestShortcutParser("internal quotes",
                       "@reboot echo \"hi\"",
                       "echo \"hi\"");
    TestShortcutParser("quoted command",
                       "@yearly true   ",
                       "true");
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_read_arys_shell_parser),
        cmocka_unit_test(test_read_freq_shell_parser),
        cmocka_unit_test(test_read_period_shell_parser),
        cmocka_unit_test(test_read_shortcut_shell_parser),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
