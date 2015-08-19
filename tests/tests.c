#include "tsf.h"

// Unit testing framework, but we are just using their convenient assert
// functions.
#include "test_helper.h"

int main(int argc, char** argv)
{
  tsf_file* tmp = tsf_open_file("does_not_exist.tsf");
  assert_non_null(tmp);
  assert_int_equal(tmp->source_count, 0);
  //fprintf(stderr, "%s\n", tmp->errmsg);
  assert_non_null(tmp->errmsg);
  tsf_close_file(tmp);

  // Assume run from parent directory where compiled to
  tsf_file* tsf = tsf_open_file("tests/low_level.tsf");
  assert_non_null(tsf);
  assert_int_equal(tsf->source_count, 1);
  assert_non_null(tsf->sources);

  tsf_source* s = &tsf->sources[0];
  // source meta
  assert_null(s->err); //Way to check for an error state for a source
  assert_int_equal(s->source_id, 1);
  assert_int_equal(s->locus_count, 4098);
  assert_int_equal(s->entity_count, -1);
  assert_string_equal(s->name, "Test Low Level");
  assert_string_equal(s->uuid, "{160faaab-81e6-48a7-bee6-24f35959b499}");
  assert_string_equal(s->date_curated, "2015-08-06 08:39:48");

  assert_string_equal(s->curated_by, "Me!");
  assert_string_equal(s->notes_html, "<h2>Genomic Index Extents</h2>\n<table cellspacing=\"0\">\n<tr><th>1</th><td>172-100063</td></tr>\n<tr><th>2</th><td>536-999692</td></tr>\n<tr><th>3</th><td>0-1093</td></tr>\n</table>\n");
  assert_string_equal(s->series_name, "test");

  assert_string_equal(s->coord_sys_id, "Made Up,Species,Chromosome");

  // fields
  assert_int_equal(s->field_count, 15);

  //check some fields
  assert_int_equal(s->fields[0].value_type, TypeEnum);
  assert_int_equal(s->fields[0].field_type, FieldLocusAttribute);
  assert_int_equal(s->fields[0].idx, -1);
  assert_string_equal(s->fields[0].name, "Chr");
  assert_string_equal(s->fields[0].symbol, "Chr");

  assert_int_equal(s->fields[1].value_type, TypeInt32);
  assert_int_equal(s->fields[1].field_type, FieldLocusAttribute);
  assert_int_equal(s->fields[1].idx, -2);
  assert_string_equal(s->fields[1].name, "Start");
  assert_string_equal(s->fields[1].symbol, "Start");

  assert_int_equal(s->fields[2].value_type, TypeInt32);
  assert_int_equal(s->fields[2].field_type, FieldLocusAttribute);
  assert_int_equal(s->fields[2].idx, -3);
  assert_string_equal(s->fields[2].name, "Stop");
  assert_string_equal(s->fields[2].symbol, "Stop");

  assert_int_equal(s->fields[3].value_type, TypeInt32);
  assert_int_equal(s->fields[3].field_type, FieldLocusAttribute);
  assert_int_equal(s->fields[3].idx, 0);
  assert_string_equal(s->fields[3].name, "Int Field");
  assert_string_equal(s->fields[3].symbol, "IntField");
  assert_float_equal(s->fields[3].extents_min, -9917.0);
  assert_float_equal(s->fields[3].extents_max, 99992.0);
  assert_int_equal(s->fields[3].enum_count, 0);

  assert_int_equal(s->fields[4].value_type, TypeInt64);
  assert_string_equal(s->fields[4].name, "Int64 Field");
  assert_string_equal(s->fields[4].symbol, "Int64Field");
  assert_float_equal(s->fields[4].extents_min, -9994000000000.0);
  assert_float_equal(s->fields[4].extents_max, 99992000000000.0);

  assert_int_equal(s->fields[5].value_type, TypeFloat32);
  assert_int_equal(s->fields[6].value_type, TypeFloat64);
  assert_int_equal(s->fields[7].value_type, TypeBool);
  assert_int_equal(s->fields[8].value_type, TypeString);
  assert_int_equal(s->fields[9].value_type, TypeInt32Array);
  assert_int_equal(s->fields[10].value_type, TypeFloat32Array);
  assert_int_equal(s->fields[11].value_type, TypeFloat64Array);
  assert_int_equal(s->fields[12].value_type, TypeStringArray);

  assert_int_equal(s->fields[13].value_type, TypeEnum);
  assert_int_equal(s->fields[13].enum_count, 4);
  assert_string_equal(s->fields[13].enum_names[0], "E1");
  assert_string_equal(s->fields[13].enum_names[1], "E2");
  assert_string_equal(s->fields[13].enum_names[2], "E3");

  assert_int_equal(s->fields[14].value_type, TypeEnumArray);
  assert_int_equal(s->fields[14].enum_count, 4);
  assert_string_equal(s->fields[14].enum_names[0], "E1");
  assert_string_equal(s->fields[14].enum_names[1], "E2");
  assert_string_equal(s->fields[14].enum_names[2], "E3");

  // Read some records
  tsf_iter* iter = tsf_query_table(tsf, 1, -1, NULL, -1, NULL);
  assert_non_null(iter);
  assert_int_equal(iter->field_count, 15);
  assert_int_equal(iter->source_id, 1);
  assert_int_equal(iter->cur_record_id, -1); //Iteration starts "before" first record
  assert_int_equal(iter->max_record_id, 4098); //Iteration starts "before" first record
  assert_int_equal(iter->is_matrix_iter, false);
  assert_int_equal(iter->entity_count, -1);
  assert_int_equal(iter->cur_entity_id, -1);

  assert_true( tsf_iter_next(iter) );

  // Validate record 0
  assert_int_equal(iter->cur_record_id, 0);

  //None of the records in this set are "null"
  for(int i=0; i<iter->field_count; i++)
    assert_true(!iter->cur_nulls[i]);

  // This is how you read "cells" of record
  assert_string_equal( v_enum_as_str(iter->cur_values[0], iter->fields[0]->enum_names), "1" );
  assert_int_equal( v_int32(iter->cur_values[1]), 89719 );
  assert_int_equal( v_int32(iter->cur_values[2]), 89760 );
  assert_int_equal( v_int32(iter->cur_values[3]), 42626 );
  assert_true( v_int64(iter->cur_values[4]) == 63418000000000LL );
  assert_float_equal( v_float32(iter->cur_values[5]), ((float)42587.9) );
  assert_float_equal( v_float64(iter->cur_values[6]), ((double)160471650054570.0) );
  assert_true( v_bool(iter->cur_values[7]) == true );
  assert_string_equal( v_str(iter->cur_values[8]), "tashcr0r1_" );
  assert_int_equal( va_size(iter->cur_values[9]), 0 );
  assert_int_equal( va_size(iter->cur_values[10]), 0 );
  assert_int_equal( va_size(iter->cur_values[11]), 4 );
  assert_float_equal( va_float64(iter->cur_values[11], 0), 13495.2);
  assert_float_equal( va_float64(iter->cur_values[11], 1), 83871.2);
  assert_float_equal( va_float64(iter->cur_values[11], 2), 63622.5);
  assert_float_equal( va_float64(iter->cur_values[11], 3), 82505.5);
  assert_int_equal( va_size(iter->cur_values[12]), 4 );
  assert_string_equal( va_str(iter->cur_values[12], 0), "_0r1htrcsa");
  assert_string_equal( va_str(iter->cur_values[12], 1), "_1rsha0trc");
  assert_string_equal( va_str(iter->cur_values[12], 2), "1s0atrr_ch");
  assert_string_equal( va_str(iter->cur_values[12], 3), "1cahr_ts0r");
  assert_int_equal( v_int32(iter->cur_values[13]), 3 );
  assert_string_equal( v_enum_as_str(iter->cur_values[13], iter->fields[13]->enum_names), "" );
  assert_int_equal( va_size(iter->cur_values[14]), 2 );
  assert_string_equal( va_enum_as_str(iter->cur_values[14], 0, iter->fields[14]->enum_names), "" );
  assert_string_equal( va_enum_as_str(iter->cur_values[14], 1, iter->fields[14]->enum_names), "" );

  // Read to another specific record
  while( tsf_iter_next(iter) ) {
    if(v_int32(iter->cur_values[1]) ==  433 &&
       v_int32(iter->cur_values[2] == 467))
      break;
  }
  assert_int_equal(iter->cur_record_id, 750);

  //None of the records in this set are "null"
  for(int i=0; i<iter->field_count; i++)
    assert_true(!iter->cur_nulls[i]);

  // Validate some of record 1
  assert_string_equal( v_enum_as_str(iter->cur_values[0], iter->fields[0]->enum_names), "1" );
  assert_int_equal( v_int32(iter->cur_values[1]), 433 );
  assert_int_equal( v_int32(iter->cur_values[2]), 467 );
  assert_int_equal( v_int32(iter->cur_values[3]), 18456 );
  assert_true( v_int64(iter->cur_values[4]) == 25166000000000LL );
  assert_float_equal( v_float32(iter->cur_values[5]), ((float)31347.8) );
  assert_float_equal( v_float64(iter->cur_values[6]), ((double)295453409885156.0) );
  assert_true( v_bool(iter->cur_values[7]) == false );
  assert_string_equal( v_str(iter->cur_values[8]), "sah1c_rr0t" );
  assert_int_equal( va_size(iter->cur_values[9]), 1 );
  assert_int_equal( va_size(iter->cur_values[10]), 0 );
  assert_int_equal( va_size(iter->cur_values[11]), 1 );
  assert_int_equal( va_size(iter->cur_values[12]), 0 );
  assert_int_equal( va_size(iter->cur_values[14]), 4 );

  // Read to another specific record
  while( tsf_iter_next(iter) ) {
    if(v_int32(iter->cur_values[1]) ==  172 &&
       v_int32(iter->cur_values[2] == 245))
      break;
  }
  assert_int_equal(iter->cur_record_id, 972);
  assert_string_equal( v_enum_as_str(iter->cur_values[0], iter->fields[0]->enum_names), "1" );
  assert_int_equal( v_int32(iter->cur_values[1]), 172 );
  assert_int_equal( v_int32(iter->cur_values[2]), 245 );
  assert_int_equal( v_int32(iter->cur_values[3]), 4390 );
  assert_true( v_int64(iter->cur_values[4]) == 45258000000000LL );
  assert_float_equal( v_float32(iter->cur_values[5]), ((float)58532.0) );
  assert_float_equal( v_float64(iter->cur_values[6]), ((double)279258246054922.0) );
  assert_true( v_bool(iter->cur_values[7]) == false );
  assert_string_equal( v_str(iter->cur_values[8]), "thr_crsa10" );

  assert_int_equal( va_size(iter->cur_values[9]), 3 );
  assert_int_equal( va_int32(iter->cur_values[9], 0), 69524 );
  assert_int_equal( va_int32(iter->cur_values[9], 1), 84891 );
  assert_int_equal( va_int32(iter->cur_values[9], 2), 79770 );

  assert_int_equal( va_size(iter->cur_values[10]), 1 );
  assert_float_equal( va_float32(iter->cur_values[10], 0), 1291.49 );

  assert_int_equal( va_size(iter->cur_values[11]), 4 );
  assert_float_equal( va_float64(iter->cur_values[11], 0), 13146.4 );
  assert_float_equal( va_float64(iter->cur_values[11], 1), 51604.6 );
  assert_float_equal( va_float64(iter->cur_values[11], 2), 58807.3 );
  assert_float_equal( va_float64(iter->cur_values[11], 3), 4838.52 );

  assert_int_equal( va_size(iter->cur_values[12]), 3 );
  assert_string_equal( va_str(iter->cur_values[12], 0), "r_ct0ar1sh" );
  assert_string_equal( va_str(iter->cur_values[12], 1), "thrcs1_0ra" );
  assert_string_equal( va_str(iter->cur_values[12], 2), "ra0tscrh1_" );

  assert_int_equal( v_int32(iter->cur_values[13]), 2 );
  assert_string_equal( v_enum_as_str(iter->cur_values[13], iter->fields[13]->enum_names), "E3" );

  assert_int_equal( va_size(iter->cur_values[14]), 4 );
  assert_string_equal( va_enum_as_str(iter->cur_values[14], 0, iter->fields[14]->enum_names), "" );
  assert_string_equal( va_enum_as_str(iter->cur_values[14], 1, iter->fields[14]->enum_names), "E1" );
  assert_string_equal( va_enum_as_str(iter->cur_values[14], 2, iter->fields[14]->enum_names), "E3" );
  assert_string_equal( va_enum_as_str(iter->cur_values[14], 3, iter->fields[14]->enum_names), "E1" );

  // Seek to a specific record
  assert_true(tsf_iter_id(iter, 4097));

  // This record was set specificially to test null values
  assert_int_equal(iter->cur_record_id, 4097);
  assert_string_equal( v_enum_as_str(iter->cur_values[0], iter->fields[0]->enum_names), "3" );
  assert_int_equal( v_int32(iter->cur_values[1]), 252 );
  assert_int_equal( v_int32(iter->cur_values[2]), 264 );
  for(int i=3; i<9; i++)
    assert_true(iter->cur_nulls[i]);
  assert_true(iter->cur_nulls[13]);
  assert_int_equal( va_size(iter->cur_values[9]), 0 );
  assert_int_equal( va_size(iter->cur_values[10]), 0 );
  assert_int_equal( va_size(iter->cur_values[11]), 0 );
  assert_int_equal( va_size(iter->cur_values[12]), 0 );
  assert_int_equal( va_size(iter->cur_values[14]), 0 );

  tsf_iter_close(iter);

  // Test subset of fields
  int* fields = calloc(sizeof(int), 4);
  fields[0] = 4;
  fields[1] = 5;
  fields[2] = 6;
  fields[3] = 8;
  iter = tsf_query_table(tsf, 1, 4, fields, -1, NULL);
  free(fields);
  assert_non_null(iter);
  assert_int_equal(iter->field_count, 4);

  assert_true( tsf_iter_next(iter) );
  assert_int_equal(iter->cur_record_id, 0);

  assert_true( v_int64(iter->cur_values[0]) == 63418000000000LL );
  assert_float_equal( v_float32(iter->cur_values[1]), ((float)42587.9) );
  assert_float_equal( v_float64(iter->cur_values[2]), ((double)160471650054570.0) );
  assert_string_equal( v_str(iter->cur_values[3]), "tashcr0r1_" );

  tsf_iter_close(iter);

  tsf_close_file(tsf);

  // TODO: Test matrix fields

  // TODO: Test gidx query
}
