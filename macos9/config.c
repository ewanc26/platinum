#include "config.h"

#include <Files.h>
#include <Folders.h>
#include <Script.h>
#include <stdlib.h>
#include <string.h>

#define PLATINUM_CONFIG_VERSION "1"
#define PLATINUM_CONFIG_MAX_FILE 8192

static const unsigned char kConfigFileName[] = {
    20, 'P','l','a','t','i','n','u','m',' ','P','r','e','f','e','r','e','n','c','e','s'
};

static OSErr platinum_config_spec(FSSpec *spec)
{
    short vRefNum;
    long dirID;
    OSErr err;

    if (spec == NULL)
        return paramErr;

    err = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder,
                     &vRefNum, &dirID);
    if (err != noErr)
        return err;

    return FSMakeFSSpec(vRefNum, dirID,
                        (ConstStr255Param)kConfigFileName, spec);
}

static int platinum_copy_string(char *dst, long capacity, const char *value)
{
    long len;

    if (dst == NULL || value == NULL || capacity <= 0)
        return 0;

    len = (long)strlen(value);
    if (len >= capacity)
        return 0;

    memcpy(dst, value, (size_t)len);
    dst[len] = '\0';
    return 1;
}

void platinum_config_init(platinum_config *config)
{
    if (config == NULL)
        return;

    memset(config, 0, sizeof(*config));
}

int platinum_config_is_paired(const platinum_config *config)
{
    if (config == NULL)
        return 0;

    return config->bridge_token[0] != '\0' &&
           config->did[0] != '\0' &&
           config->installation_id[0] != '\0';
}

OSErr platinum_config_set_bridge_url(platinum_config *config,
                                     const char *value)
{
    if (config == NULL || value == NULL)
        return paramErr;

    return platinum_copy_string(config->bridge_url,
                                PLATINUM_CONFIG_BRIDGE_URL_MAX + 1,
                                value) ? noErr : paramErr;
}

OSErr platinum_config_set_token(platinum_config *config,
                                const char *value)
{
    if (config == NULL || value == NULL)
        return paramErr;

    return platinum_copy_string(config->bridge_token,
                                PLATINUM_CONFIG_TOKEN_MAX + 1,
                                value) ? noErr : paramErr;
}

OSErr platinum_config_set_did(platinum_config *config,
                              const char *value)
{
    if (config == NULL || value == NULL)
        return paramErr;

    return platinum_copy_string(config->did,
                                PLATINUM_CONFIG_DID_MAX + 1,
                                value) ? noErr : paramErr;
}

OSErr platinum_config_set_installation_id(platinum_config *config,
                                           const char *value)
{
    if (config == NULL || value == NULL)
        return paramErr;

    return platinum_copy_string(config->installation_id,
                                PLATINUM_CONFIG_INSTALLATION_ID_MAX + 1,
                                value) ? noErr : paramErr;
}

static int platinum_append(char *buffer, long capacity, long *length,
                           const char *value)
{
    long value_len;

    if (buffer == NULL || length == NULL || value == NULL || *length < 0)
        return 0;

    value_len = (long)strlen(value);
    if (*length + value_len >= capacity)
        return 0;

    memcpy(buffer + *length, value, (size_t)value_len);
    *length += value_len;
    buffer[*length] = '\0';
    return 1;
}

static int platinum_append_line(char *buffer, long capacity, long *length,
                                const char *key, const char *value)
{
    return platinum_append(buffer, capacity, length, key) &&
           platinum_append(buffer, capacity, length, "=") &&
           platinum_append(buffer, capacity, length, value) &&
           platinum_append(buffer, capacity, length, "\r") &&
           platinum_append(buffer, capacity, length, "\n");
}

OSErr platinum_config_save(const platinum_config *config)
{
    FSSpec spec;
    short refNum;
    long length;
    OSErr err;
    char *buffer;

    if (config == NULL)
        return paramErr;

    err = platinum_config_spec(&spec);
    if (err != noErr)
        return err;

    buffer = (char *)malloc(PLATINUM_CONFIG_MAX_FILE);
    if (buffer == NULL)
        return memFullErr;

    length = 0;
    buffer[0] = '\0';

    if (!platinum_append_line(buffer, PLATINUM_CONFIG_MAX_FILE, &length,
                              "version", PLATINUM_CONFIG_VERSION) ||
        !platinum_append_line(buffer, PLATINUM_CONFIG_MAX_FILE, &length,
                              "bridge_url", config->bridge_url) ||
        !platinum_append_line(buffer, PLATINUM_CONFIG_MAX_FILE, &length,
                              "bridge_token", config->bridge_token) ||
        !platinum_append_line(buffer, PLATINUM_CONFIG_MAX_FILE, &length,
                              "did", config->did) ||
        !platinum_append_line(buffer, PLATINUM_CONFIG_MAX_FILE, &length,
                              "installation_id", config->installation_id)) {
        free(buffer);
        return paramErr;
    }

    err = FSpOpenDF(&spec, fsRdWrPerm, &refNum);
    if (err == fnfErr) {
        err = FSpCreate(&spec, 'PTLM', 'PREF', smSystemScript);
        if (err == noErr)
            err = FSpOpenDF(&spec, fsRdWrPerm, &refNum);
    }

    if (err == noErr) {
        err = SetEOF(refNum, 0);
        if (err == noErr)
            err = FSWrite(refNum, &length, buffer);
        FSClose(refNum);
    }

    free(buffer);
    return err;
}

static int platinum_key_equals(const char *line, long key_len,
                               const char *key)
{
    long expected_len;

    expected_len = (long)strlen(key);
    return key_len == expected_len &&
           memcmp(line, key, (size_t)key_len) == 0;
}

static OSErr platinum_config_parse_line(platinum_config *config,
                                         const char *line, long line_len,
                                         int *version_seen)
{
    long i;
    long key_len;
    const char *value;
    long value_len;
    char *copy;

    if (line_len <= 0)
        return noErr;

    key_len = 0;
    while (key_len < line_len && line[key_len] != '=')
        ++key_len;

    if (key_len == line_len)
        return noErr;

    value = line + key_len + 1;
    value_len = line_len - key_len - 1;

    copy = (char *)malloc((size_t)value_len + 1);
    if (copy == NULL)
        return memFullErr;

    memcpy(copy, value, (size_t)value_len);
    copy[value_len] = '\0';

    if (platinum_key_equals(line, key_len, "version")) {
        if (strcmp(copy, PLATINUM_CONFIG_VERSION) != 0) {
            free(copy);
            return paramErr;
        }
        *version_seen = 1;
    } else if (platinum_key_equals(line, key_len, "bridge_url")) {
        if (!platinum_copy_string(config->bridge_url,
                                  PLATINUM_CONFIG_BRIDGE_URL_MAX + 1,
                                  copy)) {
            free(copy);
            return paramErr;
        }
    } else if (platinum_key_equals(line, key_len, "bridge_token")) {
        if (!platinum_copy_string(config->bridge_token,
                                  PLATINUM_CONFIG_TOKEN_MAX + 1,
                                  copy)) {
            free(copy);
            return paramErr;
        }
    } else if (platinum_key_equals(line, key_len, "did")) {
        if (!platinum_copy_string(config->did,
                                  PLATINUM_CONFIG_DID_MAX + 1,
                                  copy)) {
            free(copy);
            return paramErr;
        }
    } else if (platinum_key_equals(line, key_len, "installation_id")) {
        if (!platinum_copy_string(config->installation_id,
                                  PLATINUM_CONFIG_INSTALLATION_ID_MAX + 1,
                                  copy)) {
            free(copy);
            return paramErr;
        }
    }

    free(copy);
    for (i = 0; i < line_len; ++i) {
        if (line[i] == '\0')
            break;
    }

    return noErr;
}

OSErr platinum_config_load(platinum_config *config)
{
    FSSpec spec;
    short refNum;
    long file_size;
    long bytes_read;
    long line_start;
    long i;
    OSErr err;
    int version_seen;
    char *buffer;

    if (config == NULL)
        return paramErr;

    platinum_config_init(config);

    err = platinum_config_spec(&spec);
    if (err != noErr)
        return err;

    err = FSpOpenDF(&spec, fsRdPerm, &refNum);
    if (err != noErr)
        return err;

    err = GetEOF(refNum, &file_size);
    if (err != noErr) {
        FSClose(refNum);
        return err;
    }

    if (file_size < 0 || file_size >= PLATINUM_CONFIG_MAX_FILE) {
        FSClose(refNum);
        return paramErr;
    }

    buffer = (char *)malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        FSClose(refNum);
        return memFullErr;
    }

    bytes_read = file_size;
    err = FSRead(refNum, &bytes_read, buffer);
    FSClose(refNum);
    if (err != noErr) {
        free(buffer);
        return err;
    }
    if (bytes_read != file_size) {
        free(buffer);
        return eofErr;
    }

    buffer[bytes_read] = '\0';
    version_seen = 0;
    line_start = 0;

    for (i = 0; i <= bytes_read; ++i) {
        if (i == bytes_read || buffer[i] == '\n') {
            long line_end = i;

            if (line_end > line_start && buffer[line_end - 1] == '\r')
                --line_end;

            err = platinum_config_parse_line(config, buffer + line_start,
                                             line_end - line_start,
                                             &version_seen);
            if (err != noErr) {
                free(buffer);
                return err;
            }

            line_start = i + 1;
        }
    }

    free(buffer);

    if (!version_seen)
        return paramErr;

    return noErr;
}

OSErr platinum_config_clear(void)
{
    FSSpec spec;
    OSErr err;

    err = platinum_config_spec(&spec);
    if (err != noErr)
        return err;

    err = FSpDelete(&spec);
    if (err == fnfErr)
        return noErr;
    return err;
}
