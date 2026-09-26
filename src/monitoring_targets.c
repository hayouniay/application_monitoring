#define _GNU_SOURCE

#include "monitoring_targets.h"

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* NeoTarget / NeoTargetList basics                                          */
/* ------------------------------------------------------------------------- */

void target_init(NeoTarget *target) {
  if (target == NULL) {
    return;
  }

  memset(target, 0, sizeof(*target));
  target->protocol = PROTOCOL_SSH;
}

void target_list_init(NeoTargetList *list) {
  if (list == NULL) {
    return;
  }

  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

void target_list_free(NeoTargetList *list) {
  if (list == NULL) {
    return;
  }

  free(list->items);

  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

static int target_list_reserve(NeoTargetList *list, size_t capacity) {
  NeoTarget *tmp;

  if (capacity <= list->capacity) {
    return 0;
  }

  tmp = realloc(list->items, capacity * sizeof(*tmp));

  if (tmp == NULL) {
    return -1;
  }

  list->items = tmp;
  list->capacity = capacity;

  return 0;
}

const NeoTarget *target_list_find(const NeoTargetList *list, const char *name) {
  size_t i;

  if (list == NULL || name == NULL) {
    return NULL;
  }

  for (i = 0; i < list->count; ++i) {
    if (strcmp(list->items[i].name, name) == 0) {
      return &list->items[i];
    }
  }

  return NULL;
}

int target_list_upsert(NeoTargetList *list, const NeoTarget *target) {
  size_t i;

  if (list == NULL || target == NULL || target->name[0] == '\0') {
    return -1;
  }

  for (i = 0; i < list->count; ++i) {
    if (strcmp(list->items[i].name, target->name) == 0) {
      list->items[i] = *target;
      return 0;
    }
  }

  if (list->count == list->capacity) {
    const size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;

    if (target_list_reserve(list, new_capacity) != 0) {
      return -1;
    }
  }

  list->items[list->count] = *target;
  ++list->count;

  return 0;
}

int target_list_remove(NeoTargetList *list, const char *name) {
  size_t i;

  if (list == NULL || name == NULL) {
    return -1;
  }

  for (i = 0; i < list->count; ++i) {
    if (strcmp(list->items[i].name, name) == 0) {

      memmove(&list->items[i], &list->items[i + 1],
              (list->count - i - 1) * sizeof(*list->items));

      --list->count;

      return 0;
    }
  }

  return -1;
}

/* ------------------------------------------------------------------------- */
/* Default path                                                              */
/* ------------------------------------------------------------------------- */

int targets_default_path(char *buffer, size_t buffer_size) {
  const char *xdg_config = getenv("XDG_CONFIG_HOME");
  const char *home = getenv("HOME");
  int written;

  if (buffer == NULL || buffer_size == 0) {
    return -1;
  }

  if (xdg_config != NULL && xdg_config[0] != '\0') {
    written = snprintf(buffer, buffer_size, "%s/neo-monitoring/targets.json",
                       xdg_config);
    return (written > 0 && (size_t)written < buffer_size) ? 0 : -1;
  }

  if (home == NULL || home[0] == '\0') {
    struct passwd *pw = getpwuid(getuid());

    if (pw != NULL && pw->pw_dir[0] != '\0') {
      home = pw->pw_dir;
    }
  }

  if (home == NULL || home[0] == '\0') {
    return -1;
  }

  written = snprintf(buffer, buffer_size,
                     "%s/.config/neo-monitoring/targets.json", home);

  return (written > 0 && (size_t)written < buffer_size) ? 0 : -1;
}

/* Creates every directory component of `dir` (like `mkdir -p`).
 * Pre-existing components are fine; anything else is an error. */
static int mkdir_recursive(const char *dir) {
  char path[MAX_REMOTE_PATH];
  size_t len;
  size_t i;

  snprintf(path, sizeof(path), "%s", dir);

  len = strlen(path);

  if (len == 0) {
    return -1;
  }

  for (i = 1; i < len; ++i) {

    if (path[i] != '/') {
      continue;
    }

    path[i] = '\0';

    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
      return -1;
    }

    path[i] = '/';
  }

  if (mkdir(path, 0700) != 0 && errno != EEXIST) {
    return -1;
  }

  return 0;
}

static void dirname_of(const char *path, char *out, size_t out_size) {
  const char *slash = strrchr(path, '/');

  if (slash == NULL) {
    snprintf(out, out_size, ".");
    return;
  }

  const size_t len = (size_t)(slash - path);

  if (len == 0) {
    snprintf(out, out_size, "/");
    return;
  }

  if (len + 1 > out_size) {
    out[0] = '\0';
    return;
  }

  memcpy(out, path, len);
  out[len] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Minimal JSON reader                                                       */
/*                                                                           */
/* Just enough JSON to read targets.json:                                    */
/*   { "targets": [ { "key": "value", "key2": 123, ... }, ... ] }            */
/*                                                                           */
/* Handles strings (with the common backslash escapes), numbers, true/       */
/* false/null, and arbitrarily-nested objects/arrays for values under        */
/* keys this module doesn't recognize (so a hand-edited file with extra      */
/* metadata doesn't break parsing) - but only reads the flat string/         */
/* number fields it actually understands out of each target object.         */
/* ------------------------------------------------------------------------- */

typedef struct {
  const char *data;
  size_t pos;
  size_t len;
} JsonReader;

static void json_skip_ws(JsonReader *reader) {
  while (reader->pos < reader->len &&
         isspace((unsigned char)reader->data[reader->pos])) {
    ++reader->pos;
  }
}

static bool json_peek(JsonReader *reader, char *out) {
  json_skip_ws(reader);

  if (reader->pos >= reader->len) {
    return false;
  }

  *out = reader->data[reader->pos];
  return true;
}

static bool json_consume(JsonReader *reader, char expected) {
  char c;

  if (!json_peek(reader, &c) || c != expected) {
    return false;
  }

  ++reader->pos;
  return true;
}

/* Reads a JSON string into `out` (truncating if it doesn't fit, but
 * always NUL-terminating). `out` may be NULL to just skip over it. */
static bool json_read_string(JsonReader *reader, char *out, size_t out_size) {
  size_t written = 0;

  if (!json_consume(reader, '"')) {
    return false;
  }

  while (reader->pos < reader->len && reader->data[reader->pos] != '"') {

    char c = reader->data[reader->pos++];

    if (c == '\\' && reader->pos < reader->len) {

      const char escaped = reader->data[reader->pos++];

      switch (escaped) {
      case 'n':
        c = '\n';
        break;
      case 't':
        c = '\t';
        break;
      case 'r':
        c = '\r';
        break;
      case '"':
        c = '"';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      default:
        /* Unrecognized/unicode escape: not worth a full \uXXXX decoder
         * for this config file, so keep it as a literal character. */
        c = escaped;
        break;
      }
    }

    if (out != NULL && written + 1 < out_size) {
      out[written++] = c;
    }
  }

  if (!json_consume(reader, '"')) {
    return false; /* unterminated string */
  }

  if (out != NULL) {
    out[written] = '\0';
  }

  return true;
}

static bool json_read_number(JsonReader *reader, double *out) {
  char *endptr = NULL;
  const double value = strtod(reader->data + reader->pos, &endptr);

  if (endptr == reader->data + reader->pos) {
    return false;
  }

  reader->pos += (size_t)(endptr - (reader->data + reader->pos));

  if (out != NULL) {
    *out = value;
  }

  return true;
}

static bool json_skip_value(JsonReader *reader);

static bool json_skip_collection(JsonReader *reader, char open, char close) {
  int depth = 0;

  if (!json_consume(reader, open)) {
    return false;
  }

  depth = 1;

  while (depth > 0) {

    char c;

    if (!json_peek(reader, &c)) {
      return false; /* unterminated */
    }

    if (c == '"') {
      if (!json_read_string(reader, NULL, 0)) {
        return false;
      }
      continue;
    }

    if (c == open) {
      ++depth;
      ++reader->pos;
      continue;
    }

    if (c == close) {
      --depth;
      ++reader->pos;
      continue;
    }

    ++reader->pos;
  }

  return true;
}

static bool json_skip_value(JsonReader *reader) {
  char c;

  if (!json_peek(reader, &c)) {
    return false;
  }

  if (c == '"') {
    return json_read_string(reader, NULL, 0);
  }

  if (c == '{') {
    return json_skip_collection(reader, '{', '}');
  }

  if (c == '[') {
    return json_skip_collection(reader, '[', ']');
  }

  if (strncmp(reader->data + reader->pos, "true", 4) == 0) {
    reader->pos += 4;
    return true;
  }

  if (strncmp(reader->data + reader->pos, "false", 5) == 0) {
    reader->pos += 5;
    return true;
  }

  if (strncmp(reader->data + reader->pos, "null", 4) == 0) {
    reader->pos += 4;
    return true;
  }

  return json_read_number(reader, NULL);
}

static NeoProtocol protocol_from_string(const char *text) {
  if (strcasecmp(text, "ssh") == 0) {
    return PROTOCOL_SSH;
  }

  if (strcasecmp(text, "telnet") == 0) {
    return PROTOCOL_TELNET;
  }

  if (strcasecmp(text, "ftp") == 0) {
    return PROTOCOL_FTP;
  }

  if (strcasecmp(text, "tftp") == 0) {
    return PROTOCOL_TFTP;
  }

  return PROTOCOL_LOCAL;
}

static const char *protocol_to_string(NeoProtocol protocol) {
  switch (protocol) {
  case PROTOCOL_SSH:
    return "ssh";
  case PROTOCOL_TELNET:
    return "telnet";
  case PROTOCOL_FTP:
    return "ftp";
  case PROTOCOL_TFTP:
    return "tftp";
  case PROTOCOL_LOCAL:
  default:
    return "local";
  }
}

/* Reads one `{ "key": value, ... }` target object. */
static bool json_read_target(JsonReader *reader, NeoTarget *target) {
  char key[64];
  char text[MAX_REMOTE_PATH];

  target_init(target);

  if (!json_consume(reader, '{')) {
    return false;
  }

  json_skip_ws(reader);

  {
    char c;

    if (json_peek(reader, &c) && c == '}') {
      ++reader->pos;
      return true; /* empty object: keep the all-defaults target */
    }
  }

  for (;;) {

    double number = 0.0;

    if (!json_read_string(reader, key, sizeof(key))) {
      return false;
    }

    json_skip_ws(reader);

    if (!json_consume(reader, ':')) {
      return false;
    }

    json_skip_ws(reader);

    if (strcmp(key, "name") == 0) {
      if (!json_read_string(reader, target->name, sizeof(target->name))) {
        return false;
      }

    } else if (strcmp(key, "protocol") == 0) {
      if (!json_read_string(reader, text, sizeof(text))) {
        return false;
      }
      target->protocol = protocol_from_string(text);

    } else if (strcmp(key, "host") == 0) {
      if (!json_read_string(reader, target->host, sizeof(target->host))) {
        return false;
      }

    } else if (strcmp(key, "user") == 0) {
      if (!json_read_string(reader, target->user, sizeof(target->user))) {
        return false;
      }

    } else if (strcmp(key, "password") == 0) {
      if (!json_read_string(reader, target->password,
                            sizeof(target->password))) {
        return false;
      }

    } else if (strcmp(key, "port") == 0) {
      if (!json_read_number(reader, &number)) {
        return false;
      }
      target->port = (int)number;

    } else if (strcmp(key, "identity") == 0) {
      if (!json_read_string(reader, target->identity,
                            sizeof(target->identity))) {
        return false;
      }

    } else if (strcmp(key, "remote_bin") == 0) {
      if (!json_read_string(reader, target->remote_bin,
                            sizeof(target->remote_bin))) {
        return false;
      }

    } else if (strcmp(key, "remote_file") == 0) {
      if (!json_read_string(reader, target->remote_file,
                            sizeof(target->remote_file))) {
        return false;
      }

    } else {
      /* Unknown key (forward-compat / user annotation): skip it. */
      if (!json_skip_value(reader)) {
        return false;
      }
    }

    json_skip_ws(reader);

    {
      char c;

      if (!json_peek(reader, &c)) {
        return false;
      }

      if (c == ',') {
        ++reader->pos;
        json_skip_ws(reader);
        continue;
      }

      if (c == '}') {
        ++reader->pos;
        break;
      }

      return false; /* malformed */
    }
  }

  return true;
}

static bool json_read_targets_array(JsonReader *reader, NeoTargetList *list) {
  if (!json_consume(reader, '[')) {
    return false;
  }

  json_skip_ws(reader);

  {
    char c;

    if (json_peek(reader, &c) && c == ']') {
      ++reader->pos;
      return true; /* empty array */
    }
  }

  for (;;) {

    NeoTarget target;

    if (!json_read_target(reader, &target)) {
      return false;
    }

    if (target.name[0] != '\0') {
      if (target_list_upsert(list, &target) != 0) {
        return false;
      }
    }

    json_skip_ws(reader);

    {
      char c;

      if (!json_peek(reader, &c)) {
        return false;
      }

      if (c == ',') {
        ++reader->pos;
        json_skip_ws(reader);
        continue;
      }

      if (c == ']') {
        ++reader->pos;
        break;
      }

      return false;
    }
  }

  return true;
}

static bool json_read_document(const char *text, size_t length,
                               NeoTargetList *list) {
  JsonReader reader = {text, 0, length};

  if (!json_consume(&reader, '{')) {
    return false;
  }

  json_skip_ws(&reader);

  {
    char c;

    if (json_peek(&reader, &c) && c == '}') {
      ++reader.pos;
      return true; /* "{}" - valid, empty file */
    }
  }

  for (;;) {

    char key[64];

    if (!json_read_string(&reader, key, sizeof(key))) {
      return false;
    }

    json_skip_ws(&reader);

    if (!json_consume(&reader, ':')) {
      return false;
    }

    json_skip_ws(&reader);

    if (strcmp(key, "targets") == 0) {
      if (!json_read_targets_array(&reader, list)) {
        return false;
      }
    } else {
      if (!json_skip_value(&reader)) {
        return false;
      }
    }

    json_skip_ws(&reader);

    {
      char c;

      if (!json_peek(&reader, &c)) {
        return false;
      }

      if (c == ',') {
        ++reader.pos;
        json_skip_ws(&reader);
        continue;
      }

      if (c == '}') {
        break;
      }

      return false;
    }
  }

  return true;
}

int targets_load(const char *path, NeoTargetList *list) {
  char default_path[MAX_REMOTE_PATH];
  const char *resolved_path = path;
  FILE *file;
  char *buffer;
  long file_size;
  size_t read_size;
  bool ok;

  if (list == NULL) {
    return -1;
  }

  target_list_free(list);
  target_list_init(list);

  if (resolved_path == NULL || resolved_path[0] == '\0') {

    if (targets_default_path(default_path, sizeof(default_path)) != 0) {
      return -1;
    }

    resolved_path = default_path;
  }

  file = fopen(resolved_path, "rb");

  if (file == NULL) {
    /* No saved targets yet - not an error. */
    return 0;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return -1;
  }

  file_size = ftell(file);

  if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return -1;
  }

  buffer = malloc((size_t)file_size + 1);

  if (buffer == NULL) {
    fclose(file);
    return -1;
  }

  read_size = fread(buffer, 1, (size_t)file_size, file);
  fclose(file);

  buffer[read_size] = '\0';

  ok = json_read_document(buffer, read_size, list);

  free(buffer);

  if (!ok) {
    target_list_free(list);
    return -1;
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* JSON writer                                                               */
/* ------------------------------------------------------------------------- */

static void json_write_string(FILE *file, const char *text) {
  const char *p;

  fputc('"', file);

  for (p = text; *p != '\0'; ++p) {

    switch (*p) {
    case '"':
      fputs("\\\"", file);
      break;
    case '\\':
      fputs("\\\\", file);
      break;
    case '\n':
      fputs("\\n", file);
      break;
    case '\r':
      fputs("\\r", file);
      break;
    case '\t':
      fputs("\\t", file);
      break;
    default:
      if ((unsigned char)*p < 0x20) {
        fprintf(file, "\\u%04x", (unsigned char)*p);
      } else {
        fputc(*p, file);
      }
      break;
    }
  }

  fputc('"', file);
}

int targets_save(const char *path, const NeoTargetList *list) {
  char default_path[MAX_REMOTE_PATH];
  char dir[MAX_REMOTE_PATH];
  const char *resolved_path = path;
  FILE *file;
  size_t i;

  if (list == NULL) {
    return -1;
  }

  if (resolved_path == NULL || resolved_path[0] == '\0') {

    if (targets_default_path(default_path, sizeof(default_path)) != 0) {
      return -1;
    }

    resolved_path = default_path;
  }

  dirname_of(resolved_path, dir, sizeof(dir));

  if (dir[0] != '\0' && mkdir_recursive(dir) != 0) {
    return -1;
  }

  file = fopen(resolved_path, "w");

  if (file == NULL) {
    return -1;
  }

  /* May contain a plaintext telnet/FTP password - keep it private. */
  fchmod(fileno(file), 0600);

  fputs("{\n  \"targets\": [\n", file);

  for (i = 0; i < list->count; ++i) {

    const NeoTarget *target = &list->items[i];

    fputs("    {\n", file);

    fputs("      \"name\": ", file);
    json_write_string(file, target->name);
    fputs(",\n", file);

    fputs("      \"protocol\": ", file);
    json_write_string(file, protocol_to_string(target->protocol));
    fputs(",\n", file);

    fputs("      \"host\": ", file);
    json_write_string(file, target->host);
    fputs(",\n", file);

    fputs("      \"user\": ", file);
    json_write_string(file, target->user);
    fputs(",\n", file);

    fputs("      \"password\": ", file);
    json_write_string(file, target->password);
    fputs(",\n", file);

    fprintf(file, "      \"port\": %d,\n", target->port);

    fputs("      \"identity\": ", file);
    json_write_string(file, target->identity);
    fputs(",\n", file);

    fputs("      \"remote_bin\": ", file);
    json_write_string(file, target->remote_bin);
    fputs(",\n", file);

    fputs("      \"remote_file\": ", file);
    json_write_string(file, target->remote_file);
    fputc('\n', file);

    fputs(i + 1 < list->count ? "    },\n" : "    }\n", file);
  }

  fputs("  ]\n}\n", file);

  if (fclose(file) != 0) {
    return -1;
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Bridging to/from NeoConfig                                                */
/* ------------------------------------------------------------------------- */

void target_from_config(const NeoConfig *config, const char *name,
                        NeoTarget *out) {
  if (config == NULL || out == NULL) {
    return;
  }

  target_init(out);

  if (name != NULL) {
    snprintf(out->name, sizeof(out->name), "%s", name);
  }

  out->protocol = config->protocol;

  snprintf(out->host, sizeof(out->host), "%s", config->remote_host);
  snprintf(out->user, sizeof(out->user), "%s", config->remote_user);
  snprintf(out->password, sizeof(out->password), "%s", config->remote_password);

  out->port = config->remote_port;

  snprintf(out->identity, sizeof(out->identity), "%s", config->remote_identity);
  snprintf(out->remote_bin, sizeof(out->remote_bin), "%s",
           config->remote_binary);
  snprintf(out->remote_file, sizeof(out->remote_file), "%s",
           config->remote_file);
}

void target_apply_to_config(const NeoTarget *target, NeoConfig *config) {
  if (target == NULL || config == NULL) {
    return;
  }

  config->protocol = target->protocol;

  snprintf(config->remote_host, sizeof(config->remote_host), "%s",
           target->host);
  snprintf(config->remote_user, sizeof(config->remote_user), "%s",
           target->user);
  snprintf(config->remote_password, sizeof(config->remote_password), "%s",
           target->password);

  config->remote_port = target->port;

  snprintf(config->remote_identity, sizeof(config->remote_identity), "%s",
           target->identity);
  snprintf(config->remote_binary, sizeof(config->remote_binary), "%s",
           target->remote_bin);
  snprintf(config->remote_file, sizeof(config->remote_file), "%s",
           target->remote_file);
}
