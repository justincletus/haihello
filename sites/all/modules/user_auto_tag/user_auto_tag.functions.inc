<?php

/**
 * @file
 * Auto tag users with content types tags
 *
 * User Auto Tag Module
 *
 * Drupal 7 version:
 * Written by Gianluca Agnocchetti (hiryu)
 */

/**
 * Retriving all content types.
 */
function user_auto_tag_get_node_types() {
  $results = node_type_get_types();
  foreach ((array) $results as $key => $type) {
    $types[$key] = $type->name;
  }
  return $types;
}

/**
 * Retriving fields list for a given content type.
 */
function user_auto_tag_get_fields_list($entity, $type) {
  $fields = FALSE;
  $results = user_auto_tag_multi_array_key_search("field_name", field_info_instances($entity, $type));
  array_shift($results);
  foreach ($results as $field) {
    foreach ($field as $key => $name) {
      $fields[$key] = $key;
    }
  }
  return $fields;
}

/**
 * Check if vocaboulary exist, if not, create it.
 */
function user_auto_tag_check_vocaboulary() {
  $results = taxonomy_get_vocabularies();
  foreach ($results as $vocaboulary) {
    if ($vocaboulary->machine_name == USER_AUTO_TAG_DEFAULT_VOCABULARY) {
      return TRUE;
    }
  }
  taxonomy_vocabulary_save((object) array(
    'name' => ucfirst(strtolower(USER_AUTO_TAG_DEFAULT_VOCABULARY)),
    'machine_name' => USER_AUTO_TAG_DEFAULT_VOCABULARY,
  ));
}

/**
 * Recursively searches a multidimensional array for a key and optional value.
 */
function user_auto_tag_multi_array_key_search($needle, $haystack, $strict = FALSE, $output = 'array', $value = NULL) {
  // Sanity Check.
  if (!is_array($haystack)) {
    return FALSE;
  }

  $res_idx  = 'matchedIdx';
  $prev_key = "";
  $keys     = array();

  $numargs = func_num_args();
  if ($numargs > 5) {
    $arg_list = func_get_args();
    $keys     = $arg_list[5];
    $prev_key = $arg_list[6];
  }

  $keys[$res_idx] = isset($keys[$res_idx]) ? $keys[$res_idx] : 0;

  foreach ($haystack as $key => $val) {
    if (is_array($val)) {
      if ((($key === $needle) && is_null($value)) || (($key === $needle) && ($val[$key] == $value) && $strict === FALSE) || (($key === $needle) && ($val[$key] === $value) && $strict === TRUE)) {
        if ($output == 'value') {
          $keys[$keys[$res_idx]] = $val;
        }
        else {
          $keys[$keys[$res_idx]] = $prev_key . (isset($keys[$keys[$res_idx]]) ? $keys[$keys[$res_idx]] : "") . "[\"$key\"]";
        }
        $keys[$res_idx]++;
      }
      $passed_key = $prev_key . "[\"$key\"]";
      $keys = user_auto_tag_multi_array_key_search($needle, $val, $strict, $output, $value, $keys, $passed_key);
    }
    else {
      if ((($key === $needle) && is_null($value)) || (($key === $needle) && ($val == $value) && $strict === FALSE) || (($key === $needle) && ($val === $value) && $strict === TRUE)) {
        if ($output == 'value') {
          $keys[$keys[$res_idx]] = $val;
        }
        else {
          $keys[$keys[$res_idx]] = $prev_key . (isset($keys[$keys[$res_idx]]) ? $keys[$keys[$res_idx]] : "") . "[\"$key\"]";
        }
        $keys[$res_idx]++;
      }
    }
  }
  if ($numargs < 6) {
    $keys['num_matches'] = (count($keys) == 1) ? 0 : $keys[$res_idx];
    unset($keys[$res_idx]);
    if (($output == 'array') && $keys['num_matches'] > 0) {
      $arr_keys = array(
        'num_matches' => $keys['num_matches'],
      );
      for ($i = 0; $i < $keys['num_matches']; $i++) {
        $keys_arr = explode(',', str_replace(array(
          '][',
          '[',
          ']',
        ), array(
          ',',
          '',
          '',
        ), $keys[$i]));
        $json    = "";
        foreach ($keys_arr as $nestedkey) {
          $json .= "{" . $nestedkey . ":";
        }
        $json .= is_null($value) ? "\"\"" : "\"$value\"";
        foreach ($keys_arr as $nestedkey) {
          $json .= "}";
        }
        $arr_keys[$i] = json_decode($json, TRUE);
      }
      $keys = $arr_keys;
    }
  }
  return $keys;
}
