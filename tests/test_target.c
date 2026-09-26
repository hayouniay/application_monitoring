#include "neo_test.h"

#include "monitoring_targets.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* ------------------------------------------------------------------------- */

static NeoTarget make_target(const char *name, NeoProtocol protocol,
                             const char *host) {
  NeoTarget target;
  target_init(&target);

  snprintf(target.name, sizeof(target.name), "%s", name);
  target.protocol = protocol;
  snprintf(target.host, sizeof(target.host), "%s", host);

  return target;
}

/* Unique-per-test-run scratch path so parallel `ctest -j` runs (and
 * repeated local runs) never collide with each other or with a real
 * targets.json on the machine running the suite. */
static void scratch_path(char *out, size_t out_size) {
  snprintf(out, out_size, "/tmp/neo_test_targets_%d.json", (int)getpid());
}

/* ------------------------------------------------------------------------- */
/* target_init                                                               */
/* ------------------------------------------------------------------------- */

NEO_TEST(target_init_defaults_to_ssh) {
  NeoTarget target;
  target_init(&target);

  NEO_ASSERT_EQ(target.protocol, PROTOCOL_SSH);
  NEO_ASSERT_STREQ(target.name, "");
  NEO_ASSERT_STREQ(target.host, "");
  NEO_ASSERT_EQ(target.port, 0);
}

NEO_TEST(target_init_null_is_safe) {
  target_init(NULL);
}

/* ------------------------------------------------------------------------- */
/* target_list_init / target_list_free                                      */
/* ------------------------------------------------------------------------- */

NEO_TEST(target_list_init_zeroes_the_list) {
  NeoTargetList list;
  list.items = (NeoTarget *)0x1;
  list.count = 3;
  list.capacity = 3;

  target_list_init(&list);

  NEO_ASSERT_EQ(list.items, NULL);
  NEO_ASSERT_EQ(list.count, (size_t)0);
  NEO_ASSERT_EQ(list.capacity, (size_t)0);
}

NEO_TEST(target_list_free_null_is_safe) {
  target_list_free(NULL);
}

NEO_TEST(target_list_free_resets_the_list) {
  NeoTargetList list;
  NeoTarget target = make_target("box1", PROTOCOL_SSH, "example.com");

  target_list_init(&list);
  target_list_upsert(&list, &target);

  target_list_free(&list);

  NEO_ASSERT_EQ(list.items, NULL);
  NEO_ASSERT_EQ(list.count, (size_t)0);
  NEO_ASSERT_EQ(list.capacity, (size_t)0);
}

/* ------------------------------------------------------------------------- */
/* target_list_upsert / target_list_find                                    */
/* ------------------------------------------------------------------------- */

NEO_TEST(upsert_inserts_new_target) {
  NeoTargetList list;
  NeoTarget target = make_target("box1", PROTOCOL_SSH, "example.com");
  const NeoTarget *found;

  target_list_init(&list);

  NEO_ASSERT_EQ(target_list_upsert(&list, &target), 0);
  NEO_ASSERT_EQ(list.count, (size_t)1);

  found = target_list_find(&list, "box1");

  NEO_ASSERT_TRUE(found != NULL);
  NEO_ASSERT_STREQ(found->host, "example.com");

  target_list_free(&list);
}

NEO_TEST(upsert_overwrites_existing_target_with_same_name) {
  NeoTargetList list;
  NeoTarget first = make_target("box1", PROTOCOL_SSH, "old-host.example");
  NeoTarget second = make_target("box1", PROTOCOL_TELNET, "new-host.example");
  const NeoTarget *found;

  target_list_init(&list);

  target_list_upsert(&list, &first);
  target_list_upsert(&list, &second);

  NEO_ASSERT_EQ(list.count, (size_t)1);

  found = target_list_find(&list, "box1");

  NEO_ASSERT_TRUE(found != NULL);
  NEO_ASSERT_STREQ(found->host, "new-host.example");
  NEO_ASSERT_EQ(found->protocol, PROTOCOL_TELNET);

  target_list_free(&list);
}

NEO_TEST(upsert_grows_past_initial_capacity) {
  NeoTargetList list;
  char name[TARGET_NAME_MAX];
  int i;

  target_list_init(&list);

  /* Initial capacity doubles from 0 -> 8 -> 16, so 20 entries forces
   * at least two reallocations. */
  for (i = 0; i < 20; ++i) {
    NeoTarget target;
    snprintf(name, sizeof(name), "box%d", i);
    target = make_target(name, PROTOCOL_SSH, "host");
    NEO_ASSERT_EQ(target_list_upsert(&list, &target), 0);
  }

  NEO_ASSERT_EQ(list.count, (size_t)20);
  NEO_ASSERT_TRUE(target_list_find(&list, "box0") != NULL);
  NEO_ASSERT_TRUE(target_list_find(&list, "box19") != NULL);

  target_list_free(&list);
}

NEO_TEST(upsert_rejects_empty_name) {
  NeoTargetList list;
  NeoTarget target = make_target("", PROTOCOL_SSH, "example.com");

  target_list_init(&list);

  NEO_ASSERT_EQ(target_list_upsert(&list, &target), -1);
  NEO_ASSERT_EQ(list.count, (size_t)0);

  target_list_free(&list);
}

NEO_TEST(upsert_null_arguments_are_safe) {
  NeoTargetList list;
  NeoTarget target = make_target("box1", PROTOCOL_SSH, "example.com");

  target_list_init(&list);

  NEO_ASSERT_EQ(target_list_upsert(NULL, &target), -1);
  NEO_ASSERT_EQ(target_list_upsert(&list, NULL), -1);

  target_list_free(&list);
}

NEO_TEST(find_returns_null_when_missing) {
  NeoTargetList list;
  target_list_init(&list);

  NEO_ASSERT_TRUE(target_list_find(&list, "nope") == NULL);

  target_list_free(&list);
}

NEO_TEST(find_null_arguments_are_safe) {
  NeoTargetList list;
  target_list_init(&list);

  NEO_ASSERT_TRUE(target_list_find(NULL, "box1") == NULL);
  NEO_ASSERT_TRUE(target_list_find(&list, NULL) == NULL);

  target_list_free(&list);
}

/* ------------------------------------------------------------------------- */
/* target_list_remove                                                       */
/* ------------------------------------------------------------------------- */

NEO_TEST(remove_deletes_matching_target_and_shifts_the_rest) {
  NeoTargetList list;
  NeoTarget a = make_target("a", PROTOCOL_SSH, "host-a");
  NeoTarget b = make_target("b", PROTOCOL_SSH, "host-b");
  NeoTarget c = make_target("c", PROTOCOL_SSH, "host-c");

  target_list_init(&list);
  target_list_upsert(&list, &a);
  target_list_upsert(&list, &b);
  target_list_upsert(&list, &c);

  NEO_ASSERT_EQ(target_list_remove(&list, "b"), 0);

  NEO_ASSERT_EQ(list.count, (size_t)2);
  NEO_ASSERT_TRUE(target_list_find(&list, "b") == NULL);
  NEO_ASSERT_TRUE(target_list_find(&list, "a") != NULL);
  NEO_ASSERT_TRUE(target_list_find(&list, "c") != NULL);

  target_list_free(&list);
}

NEO_TEST(remove_missing_target_returns_error) {
  NeoTargetList list;
  target_list_init(&list);

  NEO_ASSERT_EQ(target_list_remove(&list, "missing"), -1);

  target_list_free(&list);
}

NEO_TEST(remove_null_arguments_are_safe) {
  NeoTargetList list;
  target_list_init(&list);

  NEO_ASSERT_EQ(target_list_remove(NULL, "a"), -1);
  NEO_ASSERT_EQ(target_list_remove(&list, NULL), -1);

  target_list_free(&list);
}

/* ------------------------------------------------------------------------- */
/* targets_default_path                                                     */
/* ------------------------------------------------------------------------- */

NEO_TEST(default_path_prefers_xdg_config_home) {
  char buffer[MAX_REMOTE_PATH];

  setenv("XDG_CONFIG_HOME", "/tmp/xdg-scratch", 1);

  NEO_ASSERT_EQ(targets_default_path(buffer, sizeof(buffer)), 0);
  NEO_ASSERT_STREQ(buffer, "/tmp/xdg-scratch/neo-monitoring/targets.json");

  unsetenv("XDG_CONFIG_HOME");
}

NEO_TEST(default_path_falls_back_to_home) {
  char buffer[MAX_REMOTE_PATH];

  unsetenv("XDG_CONFIG_HOME");
  setenv("HOME", "/tmp/home-scratch", 1);

  NEO_ASSERT_EQ(targets_default_path(buffer, sizeof(buffer)), 0);
  NEO_ASSERT_STREQ(buffer,
                   "/tmp/home-scratch/.config/neo-monitoring/targets.json");
}

NEO_TEST(default_path_null_buffer_is_safe) {
  NEO_ASSERT_EQ(targets_default_path(NULL, 64), -1);
}

NEO_TEST(default_path_zero_size_is_safe) {
  char buffer[MAX_REMOTE_PATH];
  NEO_ASSERT_EQ(targets_default_path(buffer, 0), -1);
}

NEO_TEST(default_path_too_small_buffer_fails) {
  char buffer[8];

  setenv("XDG_CONFIG_HOME", "/tmp/a-very-long-xdg-config-home-path", 1);

  NEO_ASSERT_EQ(targets_default_path(buffer, sizeof(buffer)), -1);

  unsetenv("XDG_CONFIG_HOME");
}

/* ------------------------------------------------------------------------- */
/* targets_save / targets_load round-trip                                   */
/* ------------------------------------------------------------------------- */

NEO_TEST(save_then_load_round_trips_all_fields) {
  char path[256];
  NeoTargetList saved;
  NeoTargetList loaded;
  NeoTarget target;
  const NeoTarget *found;

  scratch_path(path, sizeof(path));
  remove(path);

  target_list_init(&saved);

  target_init(&target);
  snprintf(target.name, sizeof(target.name), "prod-box");
  target.protocol = PROTOCOL_SSH;
  snprintf(target.host, sizeof(target.host), "10.0.0.5");
  snprintf(target.user, sizeof(target.user), "ops");
  snprintf(target.password, sizeof(target.password), "s3cret");
  target.port = 2222;
  snprintf(target.identity, sizeof(target.identity), "/home/ops/.ssh/id_ed25519");
  snprintf(target.remote_bin, sizeof(target.remote_bin), "/usr/local/bin/neo");
  snprintf(target.remote_file, sizeof(target.remote_file), "/tmp/neo");

  target_list_upsert(&saved, &target);

  NEO_ASSERT_EQ(targets_save(path, &saved), 0);

  target_list_init(&loaded);
  NEO_ASSERT_EQ(targets_load(path, &loaded), 0);

  NEO_ASSERT_EQ(loaded.count, (size_t)1);

  found = target_list_find(&loaded, "prod-box");
  NEO_ASSERT_TRUE(found != NULL);
  NEO_ASSERT_EQ(found->protocol, PROTOCOL_SSH);
  NEO_ASSERT_STREQ(found->host, "10.0.0.5");
  NEO_ASSERT_STREQ(found->user, "ops");
  NEO_ASSERT_STREQ(found->password, "s3cret");
  NEO_ASSERT_EQ(found->port, 2222);
  NEO_ASSERT_STREQ(found->identity, "/home/ops/.ssh/id_ed25519");
  NEO_ASSERT_STREQ(found->remote_bin, "/usr/local/bin/neo");
  NEO_ASSERT_STREQ(found->remote_file, "/tmp/neo");

  target_list_free(&saved);
  target_list_free(&loaded);
  remove(path);
}

NEO_TEST(save_then_load_round_trips_multiple_targets_and_protocols) {
  char path[256];
  NeoTargetList saved;
  NeoTargetList loaded;
  NeoTarget ssh_target = make_target("ssh-box", PROTOCOL_SSH, "ssh-host");
  NeoTarget telnet_target =
      make_target("telnet-box", PROTOCOL_TELNET, "telnet-host");
  NeoTarget ftp_target = make_target("ftp-box", PROTOCOL_FTP, "ftp-host");
  NeoTarget tftp_target = make_target("tftp-box", PROTOCOL_TFTP, "tftp-host");

  scratch_path(path, sizeof(path));
  remove(path);

  target_list_init(&saved);
  target_list_upsert(&saved, &ssh_target);
  target_list_upsert(&saved, &telnet_target);
  target_list_upsert(&saved, &ftp_target);
  target_list_upsert(&saved, &tftp_target);

  NEO_ASSERT_EQ(targets_save(path, &saved), 0);

  target_list_init(&loaded);
  NEO_ASSERT_EQ(targets_load(path, &loaded), 0);

  NEO_ASSERT_EQ(loaded.count, (size_t)4);
  NEO_ASSERT_EQ(target_list_find(&loaded, "ssh-box")->protocol, PROTOCOL_SSH);
  NEO_ASSERT_EQ(target_list_find(&loaded, "telnet-box")->protocol,
               PROTOCOL_TELNET);
  NEO_ASSERT_EQ(target_list_find(&loaded, "ftp-box")->protocol, PROTOCOL_FTP);
  NEO_ASSERT_EQ(target_list_find(&loaded, "tftp-box")->protocol,
               PROTOCOL_TFTP);

  target_list_free(&saved);
  target_list_free(&loaded);
  remove(path);
}

NEO_TEST(load_missing_file_is_not_an_error) {
  NeoTargetList list;
  char path[256];

  scratch_path(path, sizeof(path));
  remove(path);

  target_list_init(&list);

  NEO_ASSERT_EQ(targets_load(path, &list), 0);
  NEO_ASSERT_EQ(list.count, (size_t)0);

  target_list_free(&list);
}

NEO_TEST(load_empty_document_yields_empty_list) {
  char path[256];
  FILE *file;
  NeoTargetList list;

  scratch_path(path, sizeof(path));
  file = fopen(path, "w");
  NEO_ASSERT_TRUE(file != NULL);
  fputs("{}", file);
  fclose(file);

  target_list_init(&list);
  NEO_ASSERT_EQ(targets_load(path, &list), 0);
  NEO_ASSERT_EQ(list.count, (size_t)0);

  target_list_free(&list);
  remove(path);
}

NEO_TEST(load_malformed_json_is_an_error) {
  char path[256];
  FILE *file;
  NeoTargetList list;

  scratch_path(path, sizeof(path));
  file = fopen(path, "w");
  NEO_ASSERT_TRUE(file != NULL);
  fputs("{ \"targets\": [ { \"name\": \"oops\" ", file); /* truncated */
  fclose(file);

  target_list_init(&list);
  NEO_ASSERT_EQ(targets_load(path, &list), -1);
  NEO_ASSERT_EQ(list.count, (size_t)0);

  target_list_free(&list);
  remove(path);
}

NEO_TEST(load_ignores_unknown_keys) {
  char path[256];
  FILE *file;
  NeoTargetList list;
  const NeoTarget *found;

  scratch_path(path, sizeof(path));
  file = fopen(path, "w");
  NEO_ASSERT_TRUE(file != NULL);
  fputs("{ \"version\": 2, \"targets\": [ { \"name\": \"box\", "
        "\"nickname\": \"prod\", \"meta\": { \"nested\": [1, 2, true] }, "
        "\"host\": \"h\" } ] }",
        file);
  fclose(file);

  target_list_init(&list);
  NEO_ASSERT_EQ(targets_load(path, &list), 0);
  NEO_ASSERT_EQ(list.count, (size_t)1);

  found = target_list_find(&list, "box");
  NEO_ASSERT_TRUE(found != NULL);
  NEO_ASSERT_STREQ(found->host, "h");

  target_list_free(&list);
  remove(path);
}

NEO_TEST(load_null_list_is_safe) {
  NEO_ASSERT_EQ(targets_load("/tmp/doesnt-matter.json", NULL), -1);
}

NEO_TEST(save_null_list_is_safe) {
  NEO_ASSERT_EQ(targets_save("/tmp/doesnt-matter.json", NULL), -1);
}

NEO_TEST(save_restricts_file_permissions_to_owner_only) {
  char path[256];
  NeoTargetList list;
  NeoTarget target = make_target("box", PROTOCOL_SSH, "host");
  struct stat st;

  scratch_path(path, sizeof(path));
  remove(path);

  target_list_init(&list);
  target_list_upsert(&list, &target);

  NEO_ASSERT_EQ(targets_save(path, &list), 0);

  NEO_ASSERT_EQ(stat(path, &st), 0);
  NEO_ASSERT_EQ((int)(st.st_mode & 0777), 0600);

  target_list_free(&list);
  remove(path);
}

/* ------------------------------------------------------------------------- */
/* target_from_config / target_apply_to_config                              */
/* ------------------------------------------------------------------------- */

NEO_TEST(from_config_copies_remote_fields_and_sets_name) {
  NeoConfig config;
  NeoTarget target;

  memset(&config, 0, sizeof(config));
  config.protocol = PROTOCOL_SSH;
  snprintf(config.remote_host, sizeof(config.remote_host), "10.1.1.1");
  snprintf(config.remote_user, sizeof(config.remote_user), "alice");
  snprintf(config.remote_password, sizeof(config.remote_password), "pw");
  config.remote_port = 22;
  snprintf(config.remote_identity, sizeof(config.remote_identity), "/id");
  snprintf(config.remote_binary, sizeof(config.remote_binary), "/bin/neo");
  snprintf(config.remote_file, sizeof(config.remote_file), "/tmp/f");

  target_from_config(&config, "my-target", &target);

  NEO_ASSERT_STREQ(target.name, "my-target");
  NEO_ASSERT_EQ(target.protocol, PROTOCOL_SSH);
  NEO_ASSERT_STREQ(target.host, "10.1.1.1");
  NEO_ASSERT_STREQ(target.user, "alice");
  NEO_ASSERT_STREQ(target.password, "pw");
  NEO_ASSERT_EQ(target.port, 22);
  NEO_ASSERT_STREQ(target.identity, "/id");
  NEO_ASSERT_STREQ(target.remote_bin, "/bin/neo");
  NEO_ASSERT_STREQ(target.remote_file, "/tmp/f");
}

NEO_TEST(from_config_null_name_leaves_name_empty) {
  NeoConfig config;
  NeoTarget target;

  memset(&config, 0, sizeof(config));

  target_from_config(&config, NULL, &target);

  NEO_ASSERT_STREQ(target.name, "");
}

NEO_TEST(from_config_null_arguments_are_safe) {
  NeoConfig config;
  NeoTarget target;

  memset(&config, 0, sizeof(config));

  target_from_config(NULL, "name", &target);
  target_from_config(&config, "name", NULL);
}

NEO_TEST(apply_to_config_copies_all_remote_fields) {
  NeoTarget target;
  NeoConfig config;

  target_init(&target);
  snprintf(target.host, sizeof(target.host), "host.example");
  snprintf(target.user, sizeof(target.user), "bob");
  snprintf(target.password, sizeof(target.password), "hunter2");
  target.port = 2121;
  snprintf(target.identity, sizeof(target.identity), "/key");
  snprintf(target.remote_bin, sizeof(target.remote_bin), "/bin/x");
  snprintf(target.remote_file, sizeof(target.remote_file), "/tmp/x");
  target.protocol = PROTOCOL_FTP;

  memset(&config, 0, sizeof(config));

  target_apply_to_config(&target, &config);

  NEO_ASSERT_EQ(config.protocol, PROTOCOL_FTP);
  NEO_ASSERT_STREQ(config.remote_host, "host.example");
  NEO_ASSERT_STREQ(config.remote_user, "bob");
  NEO_ASSERT_STREQ(config.remote_password, "hunter2");
  NEO_ASSERT_EQ(config.remote_port, 2121);
  NEO_ASSERT_STREQ(config.remote_identity, "/key");
  NEO_ASSERT_STREQ(config.remote_binary, "/bin/x");
  NEO_ASSERT_STREQ(config.remote_file, "/tmp/x");
}

NEO_TEST(apply_to_config_null_arguments_are_safe) {
  NeoTarget target;
  NeoConfig config;

  target_init(&target);
  memset(&config, 0, sizeof(config));

  target_apply_to_config(NULL, &config);
  target_apply_to_config(&target, NULL);
}

NEO_TEST(round_trip_from_config_then_apply_preserves_values) {
  NeoConfig original;
  NeoConfig restored;
  NeoTarget target;

  memset(&original, 0, sizeof(original));
  original.protocol = PROTOCOL_TELNET;
  snprintf(original.remote_host, sizeof(original.remote_host), "h");
  snprintf(original.remote_user, sizeof(original.remote_user), "u");
  original.remote_port = 23;

  target_from_config(&original, "t", &target);

  memset(&restored, 0, sizeof(restored));
  target_apply_to_config(&target, &restored);

  NEO_ASSERT_EQ(restored.protocol, original.protocol);
  NEO_ASSERT_STREQ(restored.remote_host, original.remote_host);
  NEO_ASSERT_STREQ(restored.remote_user, original.remote_user);
  NEO_ASSERT_EQ(restored.remote_port, original.remote_port);
}

NEO_TEST_MAIN_BEGIN()
  NEO_RUN(target_init_defaults_to_ssh);
  NEO_RUN(target_init_null_is_safe);

  NEO_RUN(target_list_init_zeroes_the_list);
  NEO_RUN(target_list_free_null_is_safe);
  NEO_RUN(target_list_free_resets_the_list);

  NEO_RUN(upsert_inserts_new_target);
  NEO_RUN(upsert_overwrites_existing_target_with_same_name);
  NEO_RUN(upsert_grows_past_initial_capacity);
  NEO_RUN(upsert_rejects_empty_name);
  NEO_RUN(upsert_null_arguments_are_safe);
  NEO_RUN(find_returns_null_when_missing);
  NEO_RUN(find_null_arguments_are_safe);

  NEO_RUN(remove_deletes_matching_target_and_shifts_the_rest);
  NEO_RUN(remove_missing_target_returns_error);
  NEO_RUN(remove_null_arguments_are_safe);

  NEO_RUN(default_path_prefers_xdg_config_home);
  NEO_RUN(default_path_falls_back_to_home);
  NEO_RUN(default_path_null_buffer_is_safe);
  NEO_RUN(default_path_zero_size_is_safe);
  NEO_RUN(default_path_too_small_buffer_fails);

  NEO_RUN(save_then_load_round_trips_all_fields);
  NEO_RUN(save_then_load_round_trips_multiple_targets_and_protocols);
  NEO_RUN(load_missing_file_is_not_an_error);
  NEO_RUN(load_empty_document_yields_empty_list);
  NEO_RUN(load_malformed_json_is_an_error);
  NEO_RUN(load_ignores_unknown_keys);
  NEO_RUN(load_null_list_is_safe);
  NEO_RUN(save_null_list_is_safe);
  NEO_RUN(save_restricts_file_permissions_to_owner_only);

  NEO_RUN(from_config_copies_remote_fields_and_sets_name);
  NEO_RUN(from_config_null_name_leaves_name_empty);
  NEO_RUN(from_config_null_arguments_are_safe);
  NEO_RUN(apply_to_config_copies_all_remote_fields);
  NEO_RUN(apply_to_config_null_arguments_are_safe);
  NEO_RUN(round_trip_from_config_then_apply_preserves_values);
NEO_TEST_MAIN_END()
