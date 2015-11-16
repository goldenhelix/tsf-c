/*-------------------------------------------------------------------------
 *
 * tsf.h
 *
 * TSF file C read-only interface.
 *
 * Copyright (c) 2012-2015 Golden Helix, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef TSF_H
#define TSF_H

// C99 expected
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
#include <math.h>

// NOTE: The Postgres coding style is nominally followed for
// type/function/variable naming and capitalization.

/*
 * Error code
 */
typedef enum {
  TSF_OK,
  TSF_ERROR,
} status_code;

/*
 * Value Type
 */
typedef enum {
  // Unit types
  TypeUnkown,
  TypeInt32,
  TypeInt64,
  TypeFloat32,
  TypeFloat64,
  TypeBool,
  TypeString,
  TypeEnum,

  // Array types
  TypeInt32Array,
  TypeFloat32Array,
  TypeFloat64Array,
  TypeBoolArray,
  TypeStringArray,
  TypeEnumArray
} tsf_value_type;

// Missing value sentinals
#ifndef INT64_MAX
#define INT64_MAX (9223372036854775807LL)
#endif //INT64_MAX

#define BOOL_MISSING   2 //Stored in 'char'
#define INT_MISSING    INT_MIN
#define INT64_MISSING  -INT64_MAX
#define FLOAT_MISSING  -FLT_MAX
#define DOUBLE_MISSING -HUGE_VAL
#define CHAR_MISSING   '\0'
#define STR_MISSING    "?"
#define GENE_MISSING   "?_?"


// Variant value
typedef void* tsf_v;

// Methods to cast variant to given type
#define v_int32(v) (*(int*)v)
#define v_int64(v) (*(int64_t*)v)
#define v_float32(v) (*(float*)v)
#define v_float64(v) (*(double*)v)
#define v_bool(v) (*(char*)v)

#define v_str(v) ((const char*)v)
#define v_enum_as_str(v, names) (names[(*(int*)v)])

// Array values can be casted to this type
typedef struct tsf_v_array {
  uint16_t size;
  char array[4];
} tsf_v_array;

typedef struct tsf_v_array_padded {
  uint16_t size;
  uint16_t padding;
  char array[4];
} tsf_v_array_padded;

#define va_size(va) ((tsf_v_array*)va)->size
#define va_array(va) ((tsf_v_array*)va)->array
#define va_array_padded(va) ((tsf_v_array_padded*)va)->array

#define va_int32(va, i) ((int*)(((tsf_v_array_padded*)va)->array))[i]
#define va_float32(va, i) ((float*)(((tsf_v_array_padded*)va)->array))[i]
#define va_float64(va, i) ((double*)(((tsf_v_array*)va)->array))[i]
#define va_bool(va, i) ((char*)(((tsf_v_array*)va)->array))[i]
#define va_enum_as_str(va, i, names) (va_int32(va, i) < 0 ? NULL : names[va_int32(va, i)])

// Strings can not be randomly accessed. They are in a NULL
// delimited list (length available with va_size)
const char* va_str(tsf_v va, int i);

/*
 * Type of records the field encodes
 */
typedef enum {
  FieldLocusAttribute,
  FieldEntityAttribute,
  FieldMatrix,
  FieldSparseArray
} tsf_field_type;

typedef struct tsf_field {
  tsf_value_type value_type;
  tsf_field_type field_type;

  int idx;             // Backend identifier of field for a given source
  const char* name;    // User displayable name
  const char* symbol;  // Follows code identifier syntax rules, unique per source

  const char* doc;
  const char* url_template;

  // Only set for enum/enumarray types
  int enum_count;
  const char** enum_names;
  const char** enum_docs;

  // Only set for numeric fields
  double extents_min;
  double extents_max;

  // Interally used by read mechanism
  int table_idx;
  const char* locus_idx_map;
  int locus_idx_map_table;
  int locus_idx_map_field;
  const char* entity_idx_map;
  int table_field_idx;
} tsf_field;

typedef struct tsf_source {
  int source_id;
  const char* name;
  const char* uuid;
  const char* err;  // If set, source is not valid for reading

  int field_count;
  tsf_field* fields;

  int entity_count;
  int locus_count;

  const char* date_curated;  // YYYY-MM-DD HH:MM:SS, such as "2015-08-06 08:39:48"

  // Documentation
  const char* curated_by;
  const char* series_name;
  const char* source_version;
  const char* description_html;
  const char* credit_html;
  const char* notes_html;
  const char* header_lines;  // Will contain '\n'

  // Specific to genomic sources
  const char* coord_sys_id;  // CoordType, Species, Build
  const char* gidx_query_table;
  const char* gidx_data_table;
  bool records_in_genomic_order;

  // Supporting source: computed off a primary
  const char* primary_source_uuid;
} tsf_source;

// Forward declare sqlite3
struct sqlite3;
struct sqlite3_stmt;

/*
 * Store meta and query state for each chunk table
 */

typedef struct tsf_chunk_table {

  int id;
  bool is_chunk_table;
  const char* name;
  int chunk_bits;
  int chunk_size;
  int field_count;
  int record_count;

  struct sqlite3_stmt* q;
} tsf_chunk_table;

/*
 * Handle for open TSF file. Opening a file parses all of its `sources`;
 */
typedef struct tsf_file {
  int source_count;  // Number of sources
  tsf_source* sources;

  int chunk_table_count; // Internal chunk table meta
  tsf_chunk_table* chunk_tables;

  char* errmsg; // Description of what went wrong

  struct sqlite3* db;
} tsf_file;

typedef enum {
  CompressionInvaid = 0x0,
  CompressionZlib   = 0x1,
  CompressionBlosc  = 0x2,
} compression_mehtod;

typedef struct tsf_chunk_header {
  //[0-1] two byte magic 0xFA01 (can also be used to indicate version in second byte in the future)
  unsigned char magic[2];

  //[2-2] One byte of flags, 2 bites used for the compression algorithm currently
  unsigned char compression_method:2;
  unsigned char dummy_padding:6;

  //[3-5] 3 bytes for type serialization. Null padded if len(format) < 3
  char format[3];

  //[5-7] 2 bytes for the size of the elements when they are uniform (i.e. strings)
  int16_t type_size;

  //[8-11] 4 byte for # of elements in chunk
  int32_t  n;

  //[12-15] 4 bytes of future use
  uint32_t future4;
} tsf_chunk_header;

#define CHUNK_MAGIC_B0 0xFA
#define CHUNK_MAGIC_B1 0x01

#define HEADER_SIZE 16 //sizeof(tsf_chunk_header)

typedef struct tsf_chunk {
  tsf_chunk_header header;

  int record_count;
  int64_t chunk_id;
  tsf_value_type value_type;
  char* chunk_data; // Just a bunch of bytes
  int chunk_bytes; //length of chunk_data

  int cur_offset;
  tsf_v cur_value;
} tsf_chunk;

/**
 * An iterator may only grab fields of a uniform FIELD_TYPE
 */
typedef struct tsf_iter {
  int cur_record_id;
  int max_record_id;  // The locus or entity count for the source

  tsf_field_type field_type; // All fields in query must be same type
  bool is_matrix_iter;  // iter_field_type == FieldTypeMatrix
  int cur_entity_idx;   // Index into entity_ids, used if is_matrix_iter

  int field_count;
  tsf_field** fields; //pointers to borrowed fields

  // These are field_count in length. Each value can be casted using TSF_
  tsf_v* cur_values;
  bool* cur_nulls;

  int entity_count;
  int* entity_ids;

  tsf_chunk* chunks;  // len <- is_matrix_iter ? field_count * entity_count :
                      // field_count
  int source_id;
  tsf_file* tsf;
} tsf_iter;

typedef struct tsf_gidx_iter {
  // Iter context, cur_record_id may not increase monotonically if source
  // is not natively in genomic order.
  tsf_iter iter;

  // Genomic query being executed
  char* chr;
  int start;
  int stop;

  // TODO: Some other sqlite held state for the current genomic index
} tsf_gidx_iter;

tsf_file* tsf_open_file(const char* fileName);

void tsf_close_file(tsf_file* tsf);

// Query the table in its natural order. Set start_id to 0 to read the
// whole table.
//
// To read all LocusAttribute fields, pass -1 as field_count and NULL to
// field_idxs, otherwise field_idxs is a field_count length array of
// field.idx values of the fields to be read.
//
// Note fields should be *ALL* the same field_type.
//
// For Matrix fields iteration, entity_count and entity_ids must be
// provided (otherwise set as -1, NULL). tsf_itr->is_matrix_iter will be
// 'true', and each record will have 'entity_count' results, with
// 'cur_entity_idx' looping from [0, entity_count].
//
// tsf_iter_next will read records into the iter until it reaches the end
// of the table.
//
// tsf_iter_close destroys the allocated iter.
tsf_iter* tsf_query_table(tsf_file* tsf, int source_id,
                          int field_count, int* field_idxs,
                          int entity_count, int* entity_ids);

// Reads cur_record_id if less than max_record_id and increment it.
// cur_values and cur_nulls are filled with the values for the record.
bool tsf_iter_next(tsf_iter* iter);

// Reads id if available, resulting it being set to iter->cur_record_id
bool tsf_iter_id(tsf_iter* iter, int id);

void tsf_iter_close(tsf_iter* iter);

// Query the genomic index (gidx) of a source.
// Returns NULL if query is source does not have a gidx
// This performs an overlap query of 0-based interval chr: (start, stop]
tsf_gidx_iter* tsf_query_genomic_index(tsf_file* tsf, int source_id,
                                       char* chr, int start, int stop,
                                       int field_count, int* field_idxs,
                                       int entity_count, int* entity_ids);


// Read the next record that overlaps the gidx query into gidx_iter->iter
bool tsf_gidx_iter_next(tsf_gidx_iter* gidx_iter);

void tsf_gidx_iter_close(tsf_gidx_iter* gidx_iter);

// TODO: Support lexicographically indexed field search

#endif
