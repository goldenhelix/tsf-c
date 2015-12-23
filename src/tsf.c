#include "tsf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <zlib.h>

// Third party libraries
#include "sqlite3/sqlite3.h"
#include "jansson/jansson.h"
#include "blosc/blosc.h"

#define RETURN_ERR(return_arg)                                                         \
  {                                                                                    \
    tsf->errmsg = malloc(strlen(fileName) + 100 + strlen(sqlite3_errmsg(tsf->db)));    \
    sprintf(tsf->errmsg, "Error opening '%s': %s", fileName, sqlite3_errmsg(tsf->db)); \
    return return_arg;                                                                 \
  }

#define PREP(q, stmt) sqlite3_prepare_v2(tsf->db, q, -1, &stmt, 0);

static char* str_dup(const char* str)
{
  int size = strlen(str) + 1;  // 1 for NULL byte
  char* dup = malloc(size);
  memcpy(dup, str, size);
  return dup;
}

static char* str_join(const char* left, const char* right, char joiner)
{
  bool use_joiner = joiner != '\0';
  int left_size = left ? strlen(left) : 0;
  int size = left_size + (use_joiner ? 1 : 0) + strlen(right) + 1;
  char* joined = malloc(size);
  if (left) {
    memcpy(joined, left, left_size);
    free((char*)left);
  }
  int offset = left_size;
  if (use_joiner)
    joined[offset++] = joiner;
  memcpy(joined + offset, right, strlen(right) + 1);
  return joined;
}

static const char* column_string_clone(sqlite3_stmt* stmt, int iCol)
{
  const unsigned char* str = sqlite3_column_text(stmt, iCol);
  int bytes = sqlite3_column_bytes(stmt, iCol);
  bytes++;  // SQLite always adds a null byte and we want to copy it as well
  unsigned char* buf = malloc(bytes);
  memcpy(buf, str, bytes);
  return (const char*)buf;
}

static tsf_value_type str_to_value_type(const char* type)
{
  if (strcmp(type, "?") == 0)
    return TypeBool;
  if (strcmp(type, "i4") == 0 || strcmp(type, "i") == 0)
    return TypeInt32;
  if (strcmp(type, "i8") == 0)
    return TypeInt64;
  if (strcmp(type, "f4") == 0 || strcmp(type, "f") == 0)
    return TypeFloat32;
  if (strcmp(type, "f8") == 0)
    return TypeFloat64;
  if (strcmp(type, "s") == 0)
    return TypeString;
  if (strcmp(type, "e") == 0)
    return TypeEnum;
  if (strcmp(type, "@i4") == 0 || strcmp(type, "@i") == 0)
    return TypeInt32Array;
  if (strcmp(type, "@f4") == 0 || strcmp(type, "@f") == 0)
    return TypeFloat32Array;
  if (strcmp(type, "@f8") == 0)
    return TypeFloat64Array;
  if (strcmp(type, "@?") == 0)
    return TypeBoolArray;
  if (strcmp(type, "@s") == 0)
    return TypeStringArray;
  if (strcmp(type, "@e") == 0)
    return TypeEnumArray;
  return TypeUnkown;
}

#define IS_DIGIT(charval) (charval >= 48 && charval <= 57)
#define IS_LETTER(charval) \
  ((charval >= 65 && charval <= 90) || (charval >= 97 && charval <= 122) || charval == '_')

bool is_code_identifier(const char* str)
{
  for (int i = 0; i < strlen(str); i++) {
    if (i == 0) {
      if (!IS_LETTER(str[i]))
        return false;
    } else {
      if (!IS_LETTER(str[i]) && !IS_DIGIT(str[i]))
        return false;
    }
  }
  return strlen(str) > 0;
}

bool field_list_contains_symbol(tsf_field* fields, int size, const char* symbol)
{
  for (int i = 0; i < size; i++)
    if (strcmp(fields[i].symbol, symbol) == 0)
      return true;
  return false;
}

// Always clone the string
const char* str_to_code_identifier(const char* str)
{
  if (is_code_identifier(str))
    return str_dup(str);

  // Remove all non-valid characters
  char* newStr = calloc(strlen(str) + 1, 1);
  char* strPtr = newStr;
  for (int i = 0; i < strlen(str); i++) {
    if (IS_DIGIT(str[i]) || IS_LETTER(str[i])) {
      *strPtr = str[i];
      strPtr++;
    }
  }

  // If empty return 'col'
  if (strlen(newStr) == 0) {
    memcpy(newStr, "col", 3);
    return newStr;
  }

  // If first char is not letter (must be a digit), prepend 'col'
  if (!IS_LETTER(newStr[0])) {
    char* p = calloc(strlen(newStr) + 4, 1);
    memcpy(p, "col", 3);
    memcpy(p + 3, newStr, strlen(newStr));
    free(newStr);
    return p;
  }

  return newStr;
}

const char* va_str(tsf_v va, int i)
{
  // Iterate through NULL seperated list to ith item
  const char* s = va_array(va);
  int count = 0;
  while (count < i) {
    while (s[0] != '\0')  // increment to next NULL
      s++;
    s++;  // go past the NULL
    count++;
  }
  return s;
}

tsf_file* tsf_open_file(const char* fileName)
{
  sqlite3* db = NULL;
  int res = sqlite3_open_v2(fileName, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, 0);
  if (db == NULL)
    return NULL;  // Should never happen, sqlite3 always sets db
  tsf_file* tsf = calloc(sizeof(tsf_file), 1);
  tsf->db = db;

  if (res != SQLITE_OK)
    RETURN_ERR(tsf);

  // Now read the sources and their meta-data
  sqlite3_stmt* q_src;
  res = PREP(
      "SELECT id, name, entity_dim, locus_dim,"
      "uuid, curated, docs, source_meta FROM source",
      q_src);

  if (res != SQLITE_OK)
    RETURN_ERR(tsf);

  sqlite3_stmt* q_field;
  res = PREP(
      "SELECT field_id, table_id, locus_idx_map, entity_idx_map, field_table_idx, "
      "field_type, field_meta "
      "FROM field WHERE source_id = ?;",
      q_field);

  if (res != SQLITE_OK)
    RETURN_ERR(tsf);

  sqlite3_stmt* q_tbl;
  res = PREP("SELECT id, table_uri, table_format, table_meta FROM tbl", q_tbl);

  if (res != SQLITE_OK)
    RETURN_ERR(tsf);

  bool has_idx_table = true;
  sqlite3_stmt* q_idx;
  res = PREP(
      "SELECT field_id, idx_type, query_table_name, data_table_id, idx_meta "
      "FROM idx WHERE source_id = ?;",
      q_idx);

  if (res != SQLITE_OK)
    has_idx_table = false;

  while (sqlite3_step(q_src) == SQLITE_ROW) {
    // Expand our sources array
    tsf_source* prev = tsf->sources;
    tsf->sources = calloc(sizeof(tsf_source), tsf->source_count + 1);
    if (tsf->source_count > 0) {
      memcpy(tsf->sources, prev, sizeof(tsf_source) * tsf->source_count);
      free(prev);
    }
    tsf->source_count++;

    tsf_source* s = &tsf->sources[tsf->source_count - 1];

    s->source_id = sqlite3_column_int(q_src, 0);
    s->name = column_string_clone(q_src, 1);
    s->entity_count = sqlite3_column_int(q_src, 2);
    if (s->entity_count == 0)
      s->entity_count = -1;  // prefer -1 as indication of "not known"
    s->locus_count = sqlite3_column_int(q_src, 3);
    if (s->locus_count == 0)
      s->locus_count = -1;
    s->uuid = column_string_clone(q_src, 4);
    s->date_curated = column_string_clone(q_src, 5);

    // Read the doc fields
    const char* docs_json = (const char*)sqlite3_column_text(q_src, 6);
    json_error_t error;
    json_t* docs = json_loads(docs_json, 0, &error);
    if (docs) {
      const char* key;
      json_t* value;
      json_object_foreach(docs, key, value)
      {
        if (strcmp(key, "curatedBy") == 0)
          s->curated_by = str_dup(json_string_value(value));
        if (strcmp(key, "seriesName") == 0)
          s->series_name = str_dup(json_string_value(value));
        if (strcmp(key, "sourceVersion") == 0)
          s->source_version = str_dup(json_string_value(value));
        if (strcmp(key, "descriptionHtml") == 0)
          s->description_html = str_dup(json_string_value(value));
        if (strcmp(key, "sourceCreditHtml") == 0)
          s->credit_html = str_dup(json_string_value(value));
        if (strcmp(key, "curationNotesHtml") == 0)
          s->notes_html = str_dup(json_string_value(value));
        if (strcmp(key, "primarySourceUuid") == 0)
          s->primary_source_uuid = str_dup(json_string_value(value));
        if (strcmp(key, "headerLines") == 0) {
          // go through each array and strcat
          char* str = 0;
          for (unsigned int i = 0; i < json_array_size(value); i++) {
            json_t* e = json_array_get(value, i);
            if (json_typeof(e) == JSON_STRING)
              str = str_join(str, json_string_value(e), '\n');
          }
          s->header_lines = str;
        }
      }
    }
    json_decref(docs);

    // Meta fields (not a lot we expect here)
    const char* meta_json = (const char*)sqlite3_column_text(q_src, 7);
    json_t* meta = json_loads(meta_json, 0, &error);
    if (meta) {
      const char* key;
      json_t* value;
      json_object_foreach(meta, key, value)
      {
        if (strcmp(key, "FeaturesInGenomicOrder") == 0)
          s->records_in_genomic_order = json_typeof(value) == JSON_TRUE;
      }
    }
    json_decref(meta);

    // Read the genomic index if available
    sqlite3_reset(q_idx);
    sqlite3_bind_int(q_idx, 1, s->source_id);
    while (sqlite3_step(q_idx) == SQLITE_ROW) {
      // int field_id = sqlite3_column_int(q_idx, 0); //useful for lexigraphical indexing
      const char* type = (const char*)sqlite3_column_text(q_idx, 1);
      const char* query_table = (const char*)sqlite3_column_text(q_idx, 2);
      const char* data_table = (const char*)sqlite3_column_text(q_idx, 3);
      const char* meta_json = (const char*)sqlite3_column_text(q_idx, 4);
      if (strcmp(type, "idx_gidx") == 0) {
        // Genomic Index
        s->gidx_query_table = str_dup(query_table);
        s->gidx_data_table = str_dup(data_table);
        json_t* meta = json_loads(meta_json, 0, &error);
        if (meta) {
          const char* key;
          json_t* value;
          json_object_foreach(meta, key, value)
          {
            if (strcmp(key, "coordSysId") == 0)
              s->coord_sys_id = str_dup(json_string_value(value));
            // TODO: Potentially grab and store "usageSpace"
          }
        }
        json_decref(meta);
      }
    }

    // Read the fields
    sqlite3_reset(q_field);
    sqlite3_bind_int(q_field, 1, s->source_id);
    while (sqlite3_step(q_field) == SQLITE_ROW) {
      // Expand our fields array
      tsf_field* prev = s->fields;
      s->fields = calloc(sizeof(tsf_field), s->field_count + 1);
      if (s->field_count > 0) {
        memcpy(s->fields, prev, sizeof(tsf_field) * s->field_count);
        free(prev);
      }
      s->field_count++;

      tsf_field* f = &s->fields[s->field_count - 1];

      f->idx = sqlite3_column_int(q_field, 0);
      f->table_idx = sqlite3_column_int(q_field, 1) - 1;
      f->locus_idx_map = column_string_clone(q_field, 2);
      f->entity_idx_map = column_string_clone(q_field, 3);
      f->table_field_idx = sqlite3_column_int(q_field, 4);  // Coerce from TEXT field

      f->value_type = str_to_value_type((const char*)sqlite3_column_text(q_field, 5));
      if (strlen(f->locus_idx_map) > 0 && strlen(f->entity_idx_map) > 0)
        f->field_type = FieldMatrix;
      else if (strlen(f->locus_idx_map) > 0)
        f->field_type = FieldLocusAttribute;
      else
        f->field_type = FieldEntityAttribute;

      if (strlen(f->locus_idx_map) > 0 && strcmp(f->locus_idx_map, "SPARSE_ARRAY") == 0)
        f->field_type = FieldSparseArray;

      f->locus_idx_map_table = -1;
      f->locus_idx_map_field = -1;
      if (strlen(f->locus_idx_map) > 0 && strcmp(f->locus_idx_map, "IDX_IS_ID") != 0) {
        // Expect it in form chunk_table_id:field_idx
        char* cursor;
        int table_id = strtol(f->locus_idx_map, &cursor, 10);
        if (cursor != f->locus_idx_map && *cursor == ':') {
          cursor++;
          char* cur = cursor;
          int field_idx = strtol(cur, &cursor, 10);
          if (cur != cursor) {
            f->locus_idx_map_table = table_id - 1;  // Index is 0-based, table_id is 1-based
            f->locus_idx_map_field = field_idx;
          }
        }
      }

      // Field meta
      const char* field_meta = (const char*)sqlite3_column_text(q_field, 6);
      json_t* meta = json_loads(field_meta, 0, &error);
      if (meta) {
        const char* key;
        json_t* value;
        json_object_foreach(meta, key, value)
        {
          if (strcmp(key, "name") == 0)
            f->name = str_dup(json_string_value(value));
          if (strcmp(key, "symbol") == 0)
            f->symbol = str_dup(json_string_value(value));
          // TODO: Could support format_flags
          // if(strcmp(key, "format") == 0)
          //  f->format_flags = str_dup(json_string_value(value));
          if (strcmp(key, "doc") == 0)
            f->doc = str_dup(json_string_value(value));
          if (strcmp(key, "urlTemplate") == 0)
            f->url_template = str_dup(json_string_value(value));
          if (strcmp(key, "enum") == 0) {
            if (f->enum_count == 0) {
              f->enum_count = json_array_size(value);
              f->enum_names = calloc(sizeof(const char*), f->enum_count);
              f->enum_docs = calloc(sizeof(const char*), f->enum_count);
            }
            for (unsigned int i = 0; i < f->enum_count; i++) {
              json_t* e = json_array_get(value, i);
              if (json_typeof(e) == JSON_ARRAY) {
                if (json_array_size(e) < 2) {
                  // Shouldn't happen, but need placeholder
                  f->enum_names[i] = str_dup("");
                  f->enum_docs[i] = str_dup("");
                  continue;
                }
                f->enum_names[i] = str_dup(json_string_value(json_array_get(e, 0)));
                json_t* enum_params_pairs = json_array_get(e, 1);
                for (int j = 0; j < json_array_size(enum_params_pairs); j++) {
                  json_t* pair = json_array_get(enum_params_pairs, j);
                  const char* key = json_string_value(json_array_get(pair, 0));
                  const char* value = json_string_value(json_array_get(pair, 0));
                  if (key && value && strcmp(key, "doc") == 0)
                    f->enum_docs[i] = str_dup(value);
                }
                if (!f->enum_docs[i])
                  f->enum_docs[i] = str_dup("");
              }
            }
          }
          if (strcmp(key, "props") == 0) {
            for (int j = 0; j < json_array_size(value); j++) {
              json_t* pair = json_array_get(value, j);
              const char* prop_key = json_string_value(json_array_get(pair, 0));
              json_t* prop_value = json_array_get(pair, 1);
              if (prop_key && prop_value && strcmp(prop_key, "ExtentsMin") == 0)
                f->extents_min = json_real_value(prop_value);
              if (prop_key && prop_value && strcmp(prop_key, "ExtentsMax") == 0)
                f->extents_max = json_real_value(prop_value);
            }
          }
        }
      }
      json_decref(meta);
    }

    // Fill in symbol if not set by source
    for (int i = 0; i < s->field_count; i++) {
      if (s->fields[i].symbol)
        continue;
      s->fields[i].symbol = str_to_code_identifier(s->fields[i].name);
      char* base_str = str_dup(s->fields[i].symbol);
      // Make sure its unique
      int count = 2;
      while (field_list_contains_symbol(s->fields, i, s->fields[i].symbol)) {
        int size = strlen(base_str) + 5;
        free((char*)s->fields[i].symbol);
        s->fields[i].symbol = calloc(size, 1);
        snprintf((char*)s->fields[i].symbol, size, "%s%d", base_str, count);
        count++;
      }
      free(base_str);
    }
  }

  // Read state and prep queries for chunk tables
  while (sqlite3_step(q_tbl) == SQLITE_ROW) {
    // Expand our chunk_tables array
    tsf_chunk_table* prev = tsf->chunk_tables;
    tsf->chunk_tables = calloc(sizeof(tsf_chunk_table), tsf->chunk_table_count + 1);
    if (tsf->chunk_table_count > 0) {
      memcpy(tsf->chunk_tables, prev, sizeof(tsf_chunk_table) * tsf->chunk_table_count);
      free(prev);
    }
    tsf->chunk_table_count++;

    tsf_chunk_table* t = &tsf->chunk_tables[tsf->chunk_table_count - 1];
    t->id = sqlite3_column_int(q_tbl, 0);
    t->is_chunk_table = strcmp((const char*)sqlite3_column_text(q_tbl, 2), "chunk_table") == 0;
    if (!t->is_chunk_table)
      continue;
    char* uri = (char*)sqlite3_column_text(q_tbl, 1);
    uri = strchr(uri, '=');
    if (uri)
      uri++;
    int len = 0;
    if (uri)
      len = strchr(uri, '&') - uri;
    if (len <= 0)
      continue;  // Unable to parse a name
    t->name = calloc(len + 1, 1);
    memcpy((char*)t->name, uri, len);

    int buflen = 57 + strlen(t->name);
    char* buf = calloc(buflen, 1);
    snprintf(buf, buflen, "SELECT chunk FROM %s WHERE chunk_id = ?", t->name);
    res = PREP(buf, t->q);
    free(buf);
    if (res != SQLITE_OK)
      RETURN_ERR(tsf);

    // Now parse the meta-data
    const char* table_meta = (const char*)sqlite3_column_text(q_tbl, 3);
    json_error_t error;
    json_t* meta = json_loads(table_meta, 0, &error);
    // Like: {"chunk_bits": 13, "field_count": 5, "record_count": 5550}
    if (meta) {
      const char* key;
      json_t* value;
      json_object_foreach(meta, key, value)
      {
        if (strcmp(key, "chunk_bits") == 0)
          t->chunk_bits = json_integer_value(value);
        if (strcmp(key, "field_count") == 0)
          t->field_count = json_integer_value(value);
        if (strcmp(key, "record_count") == 0)
          t->record_count = json_integer_value(value);
      }
    }
    json_decref(meta);
    t->chunk_size = 1 << t->chunk_bits;
  }

  sqlite3_finalize(q_src);
  sqlite3_finalize(q_tbl);
  sqlite3_finalize(q_field);
  sqlite3_finalize(q_idx);

  return tsf;
}

void tsf_close_file(tsf_file* tsf)
{
  free(tsf->errmsg);
  for (int i = 0; i < tsf->source_count; i++) {
    tsf_source* s = &tsf->sources[i];
    free((char*)s->name);
    free((char*)s->uuid);
    free((char*)s->err);
    free((char*)s->date_curated);
    free((char*)s->curated_by);
    free((char*)s->series_name);
    free((char*)s->source_version);
    free((char*)s->description_html);
    free((char*)s->credit_html);
    free((char*)s->notes_html);
    free((char*)s->header_lines);
    free((char*)s->coord_sys_id);
    free((char*)s->gidx_query_table);
    free((char*)s->gidx_data_table);
    free((char*)s->primary_source_uuid);
    for (int j = 0; j < s->field_count; j++) {
      tsf_field* f = &s->fields[j];
      free((char*)f->name);
      free((char*)f->symbol);
      free((char*)f->doc);
      free((char*)f->url_template);
      for (int k = 0; k < f->enum_count; k++) {
        free((char*)f->enum_names[k]);
        free((char*)f->enum_docs[k]);
      }
      free(f->enum_names);
      free(f->enum_docs);
      free((char*)f->locus_idx_map);
      free((char*)f->entity_idx_map);
    }
    free(s->fields);
  }
  free(tsf->sources);

  for (int i = 0; i < tsf->chunk_table_count; i++) {
    free((char*)tsf->chunk_tables[i].name);
    sqlite3_finalize(tsf->chunk_tables[i].q);
  }
  free(tsf->chunk_tables);

  int res = sqlite3_close_v2(tsf->db);
  if (res == SQLITE_BUSY)
    fprintf(stderr, "TSF SQLite database failed to close because of un-finalized() statements");
  free(tsf);
}

void* error(const char* msg)
{
  fprintf(stderr, "%s\n", msg);
  return NULL;
}

tsf_iter* tsf_query_table(tsf_file* tsf, int source_id, int field_count, int* field_idxs,
                          int entity_count, int* entity_ids, tsf_field_type field_type)
{
  if (!tsf)
    return NULL;

  // Prepare some queries before we continue
  tsf_iter* iter = calloc(sizeof(tsf_iter), 1);
  iter->tsf = tsf;
  iter->source_id = source_id;
  iter->cur_record_id = -1;
  iter->entity_count = -1;
  iter->cur_entity_idx = -1;
  iter->is_matrix_iter = false;
  iter->field_type = field_type;

  tsf_source* s = &tsf->sources[source_id - 1];

  // Set up iter->field_count and iter->fields
  if (field_count < 0) {
    // Default to all locus attribute fields
    iter->field_count = 0;
    iter->fields = calloc(sizeof(tsf_file*), s->field_count);
    for (int i = 0; i < s->field_count; i++) {
      if (s->fields[i].field_type == iter->field_type)
        iter->fields[iter->field_count++] = &s->fields[i];
    }
  } else {
    iter->field_count = field_count;
    iter->fields = calloc(sizeof(tsf_file*), iter->field_count);
    if(field_count > 0 && iter->field_type == FieldTypeInvalid)
      iter->field_type = s->fields[field_idxs[0]].field_type;
    for (int i = 0; i < iter->field_count; i++) {
      iter->fields[i] = &s->fields[field_idxs[i]];
      if (iter->field_type != s->fields[field_idxs[0]].field_type)
        return error("All fields passed into tsf_query_table must have a consistent field_type");
    }
  }

  iter->is_matrix_iter = (iter->field_type == FieldMatrix);
  if (iter->field_type == FieldEntityAttribute)
    iter->max_record_id = s->entity_count;
  else
    iter->max_record_id = s->locus_count;

  if (iter->is_matrix_iter) {
    if (entity_count <= 0) {
      // Default to all entities
      iter->entity_count = s->entity_count;
      iter->entity_ids = malloc(sizeof(int) * s->entity_count);
      for (int i = 0; i < iter->entity_count; i++)
        iter->entity_ids[i] = i;
    } else {
      iter->entity_count = entity_count;
      iter->entity_ids = malloc(sizeof(int) * entity_count);
      memcpy(iter->entity_ids, entity_ids, sizeof(int) * entity_count);
    }
  }

  iter->cur_values = calloc(sizeof(tsf_v), iter->field_count);
  iter->cur_nulls = calloc(sizeof(bool), iter->field_count);
  int chunk_count =
      iter->is_matrix_iter ? iter->field_count * iter->entity_count : iter->field_count;
  iter->chunks = calloc(sizeof(tsf_chunk), chunk_count);

  // Intialize chunk_id to an invalid number (0 is valid).
  for (int i = 0; i < chunk_count; i++)
    iter->chunks[i].chunk_id = -1;

  return iter;
}

static int zlib_expcted_size(const unsigned char* data)
{
  if (!data)
    return -1;
  unsigned int expectedSize = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
  return (int)expectedSize;
}

static bool zlib_uncompress(unsigned char* dest, unsigned long expectedSize,
                            const unsigned char* data, unsigned long nbytes)
{
  if (!data)
    return false;

  // 4 is the size header in the beginning of our compressed buffer
  int res = uncompress(dest, &expectedSize, data + 4, nbytes - 4);

  switch (res) {
    case Z_OK:
      return true;
    case Z_MEM_ERROR:
      return (bool)error("zlib_uncompress: Z_MEM_ERROR: Not enough memory");
    case Z_BUF_ERROR:
      return (bool)error(
          "zlib_uncompress: Z_BUF_ERROR: expectedSize was not enough bytes to compress into");
    case Z_DATA_ERROR:
      return (bool)error("zlib_uncompress: Z_DATA_ERROR: Input data is corrupted");
  }
  return true;
}

static bool read_chunk(tsf_chunk_table* t, tsf_chunk* c, int64_t chunk_id)
{
  sqlite3_reset(t->q);
  sqlite3_bind_int64(t->q, 1, chunk_id);
  if (sqlite3_step(t->q) != SQLITE_ROW)
    return false;
  const char* raw_data = (const char*)sqlite3_column_blob(t->q, 0);
  int size = sqlite3_column_bytes(t->q, 0);

  if (size < HEADER_SIZE) {
    return (bool)error("Less than 16 bytes expected for header of chunk");
  }
  memcpy(&c->header, raw_data, HEADER_SIZE);
  if (c->header.magic[0] != CHUNK_MAGIC_B0 || c->header.magic[1] != CHUNK_MAGIC_B1) {
    return (bool)error(
        "Chunk did not start with expected magic 2 bytes. Possibly corrupted or created with newer "
        "software.");
  }

  // Because header is 3 bytes, with no NULL terminator, need to put it in a 4 byte tmp
  char* tmp_value_type = calloc(4, 1);
  memcpy(tmp_value_type, c->header.format, 3);
  c->value_type = str_to_value_type(tmp_value_type);
  free(tmp_value_type);
  if (c->value_type == TypeUnkown)
    return (bool)error("Unexpected format string in chunk");

  free(c->chunk_data);
  c->chunk_data = 0;
  c->chunk_bytes = 0;
  c->chunk_id = chunk_id;
  c->record_count = c->header.n;
  c->cur_offset = 0;

  // decompress chunk
  if (c->header.compression_method == CompressionZlib) {
    if (size < (HEADER_SIZE + 4))
      return true;  // empty chunk

    const char* data = raw_data + HEADER_SIZE;
    c->chunk_bytes = zlib_expcted_size((const unsigned char*)data);
    c->chunk_data = malloc(c->chunk_bytes);
    if (!zlib_uncompress((unsigned char*)c->chunk_data, c->chunk_bytes, (const unsigned char*)data,
                         size - HEADER_SIZE))
      return false;
  } else if (c->header.compression_method == CompressionBlosc) {
    if (size < (HEADER_SIZE + BLOSC_MIN_HEADER_LENGTH))
      return true;  // empty chunk

    const char* data = raw_data + HEADER_SIZE;
    size_t nbytes, cbytes, blocksize;
    blosc_cbuffer_sizes(data, &nbytes, &cbytes, &blocksize);
    if (cbytes != size - HEADER_SIZE)
      return error("BLOSC buffer or header corrupt");

    c->chunk_bytes = nbytes;
    c->chunk_data = malloc(c->chunk_bytes);
    int err = blosc_decompress(data, c->chunk_data, c->chunk_bytes);
    if (err < 0 || err != (int)nbytes)
      return error("Chunk had BLOSC error while decompressing");
  } else {
    return error("Unkown compression method of chunk");
  }

  // set up pointer at beginning of chunk
  c->cur_value = (tsf_v)c->chunk_data;
  return true;
}

#define READ_FIXED(type_t, v_read, v_missing)       \
  c->cur_offset = offset;                           \
  c->cur_value = &((type_t*)c->chunk_data)[offset]; \
  *value = c->cur_value;                            \
  *is_null = (v_read(c->cur_value) == v_missing);

void chunk_value(tsf_chunk* c, int offset, tsf_v* value, bool* is_null)
{
  // Read the typed value of chunk at offset, setting value to
  // appropriate place in chunk->chunk_data and is_null appropriately.

  bool pad_size = false;  // Used by typed arrays
  switch (c->value_type) {
    // Random access types
    case TypeInt32:
      READ_FIXED(int32_t, v_int32, INT_MISSING);
      break;
    case TypeInt64:
      READ_FIXED(int64_t, v_int64, INT64_MISSING);
      break;
    case TypeFloat32:
      READ_FIXED(float, v_float32, FLOAT_MISSING);
      break;
    case TypeFloat64:
      READ_FIXED(double, v_float64, DOUBLE_MISSING);
      break;
    case TypeBool:
      READ_FIXED(char, v_bool, BOOL_MISSING);
      break;
    case TypeEnum:  // Data is TypeInt32
      READ_FIXED(int32_t, v_int32, INT_MISSING);
      break;

    // Variable length types
    case TypeString: {
      // String chunks may be uniformly sized strings of size
      // header.type_size or a NULL delimited list.
      if (c->header.type_size == 0) {
        // NULL delimited string list

        // Most times, we are iterating through chunks and we expect offset to
        // be > c->cur_offset, if not, we need to start from the beginning.
        if (c->cur_offset > offset) {
          c->cur_offset = 0;
          c->cur_value = (tsf_v)c->chunk_data;
        }
        const char* s = (const char*)c->cur_value;
        const char* end = ((const char*)c->chunk_data) + c->chunk_bytes;

        // Iter as many NULL bytes as the number os strings to advance
        while (c->cur_offset < offset) {
          assert(s < end);      // Should stop at last null-terminated string
          while (s[0] != '\0')  // increment to next NULL
            s++;
          s++;  // go past the NULL
          c->cur_offset++;
        }
        c->cur_value = (tsf_v)s;
        *value = c->cur_value;
        *is_null = s[0] == '\0' || (s[0] == '?' && s[1] == '\0');
      } else {
        // Uniformly sized string list
        c->cur_offset = offset;
        const char* s = &((const char*)c->chunk_data)[offset * c->header.type_size];
        c->cur_value = (tsf_v)s;
        *value = c->cur_value;
        *is_null = s[0] == '\0' || (s[0] == '?' && s[1] == '\0');
      }
      break;
    }
    case TypeInt32Array:    // fallthrough
    case TypeEnumArray:     // fallthrough (data is int-array)
    case TypeFloat32Array:  // fallthrough
      pad_size = true;      // These 4-byte arrays are kept padded to 4-byte boundries
    case TypeFloat64Array:  // fallthrough
    case TypeBoolArray:     // fallthrough
    {
      // Most times, we are iterating through chunks and we expect offset to
      // be > c->cur_offset, if not, we need to start from the beginning.
      if (c->cur_offset > offset) {
        c->cur_offset = 0;
        c->cur_value = (tsf_v)c->chunk_data;
      }

      const char* s = (const char*)c->cur_value;
      const char* end = ((const char*)c->chunk_data) + c->chunk_bytes;

      // Iter as many elements as the number to advance
      while (c->cur_offset < offset) {
        assert(s < end);
        uint16_t size = va_size(s);
        // Move past size, which is sometimes padded to type_size
        s += (pad_size ? c->header.type_size : sizeof(uint16_t));
        s += size * c->header.type_size;
        c->cur_offset++;
      }
      c->cur_value = (tsf_v)s;
      *value = c->cur_value;
      *is_null = false;  // Array types are not null, just empty
      break;
    }
    case TypeStringArray: {
      // Most times, we are iterating through chunks and we expect offset to
      // be > c->cur_offset, if not, we need to start from the beginning.
      if (c->cur_offset > offset) {
        c->cur_offset = 0;
        c->cur_value = (tsf_v)c->chunk_data;
      }
      const char* s = (const char*)c->cur_value;
      const char* end = ((const char*)c->chunk_data) + c->chunk_bytes;

      // Iter as many elements as the number to advance
      while (c->cur_offset < offset) {
        assert(s < end);
        uint16_t size = va_size(s);
        s += sizeof(uint16_t);  // Move past size
        for (int j = 0; j < size; j++) {
          assert(s < end);
          while (s[0] != '\0')  // increment to next NULL
            s++;
          s++;  // go past the NULL
        }
        c->cur_offset++;
      }
      c->cur_value = (tsf_v)s;
      *value = c->cur_value;
      *is_null = false;  // Array types are not null, just empty
      break;
    }
    case TypeUnkown:
      return;
  }
}

bool read_chunk_with_idxmap(tsf_file* tsf, tsf_chunk* c, tsf_field* f, int record_id, int field_idx)
{
  // If we have no idx_map for locus dimention, do a strait read_chunk
  tsf_chunk_table* t = &tsf->chunk_tables[f->table_idx];
  int64_t chunk_id = ((int64_t)(record_id >> t->chunk_bits) << 32) | field_idx;
  if (f->locus_idx_map_table < 0) {
    return read_chunk(t, c, chunk_id);
  }

  // Only Chr, Start, Stop genomic fields uses the field_idx_map in TSF1
  if (f->value_type != TypeInt32 && f->value_type != TypeEnum)
    return (bool)error("Currently only Int/Enum fields support locux_idx_map being set");

  // Otherwise, handle cases where there is a index mapping between the ID space we
  // are reading and the final records.
  // First, we read the idx chunk, which should be a integer chunk
  tsf_chunk idx_chunk;
  memset(&idx_chunk, 0, sizeof(tsf_chunk));
  tsf_chunk_table* idx_chunk_table = &tsf->chunk_tables[f->locus_idx_map_table];
  int64_t idx_chunk_id =
      ((int64_t)(record_id >> idx_chunk_table->chunk_bits) << 32) | f->locus_idx_map_field;
  if (!read_chunk(idx_chunk_table, &idx_chunk, idx_chunk_id))
    return false;

  // Set up our passed in chunk with values filled in from the indexed
  // backend chunks.
  free(c->chunk_data);
  c->header = idx_chunk.header;
  c->record_count = c->header.n;
  c->value_type = TypeInt32;
  c->chunk_id = chunk_id;  // One compared against in the iter_next
  c->cur_offset = 0;
  c->chunk_bytes = c->header.type_size * c->record_count;
  c->chunk_data = malloc(c->chunk_bytes);
  c->cur_value = (tsf_v)c->chunk_data;

  // Worst case is we have one chunk per record in our idx chunk
  int backend_chunks_count = 0;
  tsf_chunk* backend_chunks = calloc(sizeof(tsf_chunk), idx_chunk.record_count);
  tsf_v value;
  bool is_null;
  for (int i = 0; i < idx_chunk.record_count; i++) {
    chunk_value(&idx_chunk, i, &value, &is_null);
    int idx = v_int32(value);
    int64_t chunk_id = ((int64_t)(idx >> t->chunk_bits) << 32) | field_idx;
    int offset = idx % t->chunk_size;

    // Linearly scan for this chunk_id in our existing backend_chunks
    int j;
    for (j = 0; j < backend_chunks_count; j++) {
      if (backend_chunks[j].chunk_id == chunk_id) {
        break;
      }
    }
    int chunk_idx = j;  // Either found or set to backend_chunks_count

    // Not found, fetch this chunk
    if (chunk_idx >= backend_chunks_count) {
      backend_chunks_count++;
      read_chunk(t, &backend_chunks[chunk_idx], chunk_id);
    }

    // Read backend chunk value into our collated chunk data
    chunk_value(&backend_chunks[chunk_idx], offset, &value, &is_null);
    ((int*)c->chunk_data)[i] = v_int32(value);
  }
  free(idx_chunk.chunk_data);
  for (int i = 0; i < backend_chunks_count; i++)
    free(backend_chunks[i].chunk_data);
  free(backend_chunks);
  return true;
}

static bool tsf_iter_read_current(tsf_iter* iter)
{
  // Copy appropriate values into cur_values
  for (int i = 0; i < iter->field_count; i++) {
    tsf_field* f = iter->fields[i];
    tsf_chunk_table* t = &iter->tsf->chunk_tables[f->table_idx];
    tsf_chunk* c;
    if (iter->is_matrix_iter)
      c = &iter->chunks[(i * iter->entity_count) + iter->cur_entity_idx];
    else
      c = &iter->chunks[i];

    // For matrix fields, the chunk ID field_idx is the entity offset
    int field_idx = iter->is_matrix_iter ? iter->entity_ids[iter->cur_entity_idx] : f->table_field_idx;

    int64_t chunk_id = ((int64_t)(iter->cur_record_id >> t->chunk_bits) << 32) | field_idx;
    int offset = iter->cur_record_id % t->chunk_size;

    if (c->chunk_id != chunk_id)
      if (!read_chunk_with_idxmap(iter->tsf, c, f, iter->cur_record_id, field_idx))
        return false;

    // Need to set cur_values and cur_nulls to appropriate values
    chunk_value(c, offset, &iter->cur_values[i], &iter->cur_nulls[i]);
  }
  return true;
}

bool tsf_iter_next(tsf_iter* iter)
{
  if (!iter->is_matrix_iter) {
    // No entity dimention. Each iter_next increements cur_record_id
    iter->cur_record_id++;
  } else {
    iter->cur_entity_idx++;

    // Loop around
    if (iter->cur_entity_idx >= iter->entity_count)
      iter->cur_entity_idx = 0;

    // Otherwise, only when we loop around to cur_entity_id == 0 do we increment record
    if (iter->cur_entity_idx == 0)
      iter->cur_record_id++;
  }

  // Finished iteration
  if (iter->cur_record_id >= iter->max_record_id)
    return false;

  return tsf_iter_read_current(iter);
}

bool tsf_iter_id(tsf_iter* iter, int id)
{
  if(iter->cur_record_id == id)
    return true; // Don't do work
  if(id < 0)
    return false; // Invalid

  if(iter->is_matrix_iter)
    iter->cur_entity_idx = 0; //Reset cur_entity_idx, otherwise call tsf_iter_id_matrix

  iter->cur_record_id = id;
  if(iter->cur_record_id >= iter->max_record_id)
    return false; //end of table

  return tsf_iter_read_current(iter);
}

bool tsf_iter_id_matrix(tsf_iter* iter, int id, int entity_idx)
{
  if(iter->cur_record_id == id && iter->cur_entity_idx == entity_idx)
    return true; // Don't do work
  if(id < 0 || entity_idx < 0)
    return false; // Invalid
  assert(iter->is_matrix_iter);
  assert(entity_idx >= 0 && entity_idx < iter->entity_count);

  iter->cur_record_id = id;
  iter->cur_entity_idx = entity_idx;
  if(iter->cur_record_id >= iter->max_record_id)
    return false; //end of table

  return tsf_iter_read_current(iter);
}

void tsf_iter_close(tsf_iter* iter)
{
  if(!iter)
    return;
  free(iter->fields);
  free(iter->entity_ids);
  free(iter->cur_values);
  free(iter->cur_nulls);
  int chunk_count =
      iter->is_matrix_iter ? iter->field_count * iter->entity_count : iter->field_count;
  for (int i = 0; i < chunk_count; i++)
    free(iter->chunks[i].chunk_data);
  free(iter->chunks);
  free(iter);
}
