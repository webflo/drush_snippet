<?php
/**
 * @file
 * Snippet/template generator for Drush
 */

/**
 * Implements hook_drush_command().
 */
function snippet_drush_command() {
  $items = array();

  $items['snippet-generate'] = array(
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'description' => "Generate snippets.",
    'options' => array(
      'output' => 'A directory of file',
      'type' => 'hook or theme',
      'format' => 'format plugin'
    ),
    'examples' => array(
      'Hooks for PhpStorm' => 'drush snippet-generate --type=hook --output=/tmp/d7 --format=PhpStorm',
      'Theme function so for TextMate' => 'drush snippet-generate --type=theme --output=/tmp/d6 --format=TextMate',
    ),
  );

  return $items;
}

/**
 * Implements of hook_drush_help().
 *
 */
function snippet_drush_help($section) {
  switch ($section) {
    case 'drush:snippet-generate':
      return dt("Generate snippets from your Drupal installation.");
  }
}

/**
 * Drush TextMate-generate command callback.
 */
function drush_textmate_tm_generate() {
  $version = drush_drupal_major_version();
  $type = drush_get_option('type');
  if (is_string(drush_get_option('write')) && $dir = realpath(drush_get_option('write'))) {
    if (is_writable($dir)) {
      $write_files = TRUE;
    }
  }

  $functions = drush_textmate_find_functions($type, $version);

  foreach($functions as $function_name) {
    if (strpos($function_name, '::') === FALSE) {
      if (!function_exists($function_name)) {
        return drush_set_error(dt('Function not found'));
      }
      $reflect = new ReflectionFunction($function_name);
    }
    else {
      list($class, $method) = explode('::', $function_name);
      if (!method_exists($class, $method)) {
        return drush_set_error(dt('Method not found'));
      }
      $reflect = new ReflectionMethod($class, $method);
    }

    $function = _textmate_drush_print_function($type, $function_name, $reflect->getFileName(), $reflect->getStartLine(), $reflect->getEndLine());
    if (isset($write_files)) {
      file_put_contents($dir . '/' . $function['name'] . '.' . $version . '.php', $function['code']);
    } else {
      drush_print_r($function);
    }
  }
}

/**
 * Generate snippets. Drush callback.
 */
function drush_snippet_generate() {
  require_once dirname(__FILE__) . '/SnippetGenerator.inc';

  $version = drush_drupal_major_version();
  $type = drush_get_option('type');
  $dir = drush_get_option('output');
  $format = drush_get_option('format');

  $class = 'SnippetGenerator' . $format;
  $generator = new $class($type, $version);
  $generator->find();
  $functions = $generator->parse();
  $generator->write($dir, $functions);
}

/**
 * Drush TextMate-format command callback.
 */
function drush_textmate_tm_format() {
  $version = drush_get_option('version');
  $textmate = drush_get_option('textmate');
  $type = drush_get_option('type');
  if (is_string(drush_get_option('write')) && $dir = realpath(drush_get_option('write'))) {
    if (is_writable($dir)) {
      $write_files = TRUE;
    }
  }

  $existing_commands = array();
  if (!empty($textmate)) {
    $scan = array();
    foreach(drush_scan_directory($textmate . '/Commands', '/\.tmCommand/', array('.', '..', 'CVS')) as $file) {
      list($file_type) = explode('_', $file->basename);
      if ($file_type == $type) {
        $existing_commands[$file->name] = $file->name;
      }
    }
  }

  $output = array();

  if (!empty($textmate)) {
    $scan += drush_scan_directory($textmate . '/Support/commands/custom/hooks', '/\.php/', array('.', '..', 'CVS'));
    $scan += drush_scan_directory($textmate . '/Support/commands/generated/hooks', '/\.php/', array('.', '..', 'CVS'));
    $scan += drush_scan_directory($textmate . '/Support/commands/custom/theme', '/\.php/', array('.', '..', 'CVS'));
    $scan += drush_scan_directory($textmate . '/Support/commands/generated/theme', '/\.php/', array('.', '..', 'CVS'));
  }

  $function = array();
  foreach($scan as $file) {
    list($file_function, $file_version) = explode('.', $file->basename);
    list($file_type) = explode('_', $file->basename);
    if (($file_type == 'hook' || $file_type == 'theme') && (($file_version == $version) || !is_numeric($file_version))) {
      $function[$file_function] = $file_function;
    }
  }

  // Delete some lowercase function.
  // Example: delete hook_form_form_id_alter and preserve hook_form_FORM_ID_alter.
  foreach($function as $name) {
    $lower = strtolower($name);
    if ($name != $lower && in_array($lower, $function)) {
      $delete = array_search($lower, $function);
      unset($function[$delete]);
    }
  }

  switch (drush_get_option('filter')) {
    case 'new':
      $function = array_diff($function, $existing_commands);
      break;

    case 'existing':
      $function = array_intersect($function, $existing_commands);
      break;
  }

  sort($function);

  if (drush_get_option('format') == 'plist') {
    foreach($function as $name) {
      $output[] = '{display = '  . "'" . $name . "'" . '; }';
    }
    $output = "(\n" . implode(",\n", $output) . "\n)";
  } elseif (drush_get_option('format') == 'txt') {
    $output = implode("\n", $function);
  }
  drush_print_r($output);
}

/**
 * undocumented function
 *
 * @param string $type
 *  Function type. hook or theme
 * @param int $version
 *  Drupal major version.
 */
function drush_textmate_find_functions($type, $version) {
  $scan = array();
  switch ($type) {
    case 'hook':
      switch ($version) {
        case 7:
          $scan = $scan + drush_scan_directory('./modules', '/\.api\.php$/', _textmate_drush_filter_files());
          break;

        case 6:
          $scan = $scan + drush_scan_directory('./docs', '/\.php$/', _textmate_drush_filter_files());
          break;

        // case 5:
        //   $scan = $scan + drush_scan_directory('./d5-hooks', '/\.php$/', _textmate_drush_filter_files());
        //   break;

        default:
          print 'Invalid drupal version.';
          exit;
          break;
      }

      // $scan = $scan + drush_scan_directory('./modules', '/\.php$/', array('.', '..', 'CVS', '.tpl.php'));
      $scan = $scan + drush_scan_directory('./modules', '/\.inc$/', _textmate_drush_filter_files());
      $scan = $scan + drush_scan_directory('./modules', '/\.module$/', _textmate_drush_filter_files());

      foreach($scan as $file) {
        if (substr($file->basename, -7) != 'tpl.php') {
          include_once $file->filename;
        }
      }
      break;

    case 'theme':
      init_theme();
      $theme_registry = theme_get_registry();
      break;

    default:
      print 'Invalid type.';
      exit;
  }

  $functions = get_defined_functions();
  $functions = array_values(array_filter($functions['user'], '_textmate_drush_' . $type . '_filter'));
  sort($functions);
  return $functions;
}

/**
 * Print the specified function
 */
function _textmate_drush_print_function($type, $function_name, $file, $start_line, $end_line) {
  $line_num = 0;
  $doxygen = NULL;
  $fp = fopen($file, 'r');

  $function = '';

  while (!feof($fp) && ($line_num < ($start_line - 1))) {
    $line = fgets($fp);
    ++$line_num;

    if (substr($line,0,3) == '/**') {
      $doxygen = $line;
    }
    elseif (isset($doxygen)) {
      $doxygen .= $line;
      if ($line_num + 1 == $start_line) {
        // drush_print(rtrim($doxygen));
      }
      if (strstr($line, '*/') !== FALSE) {
        $doxygen = NULL;
      }
    }
  }

  $line_function_start = $line_num;
  $start_escape = $line_num + 1;
  $end_escape = $end_line - 1;

  $escape = FALSE;
  while (!feof($fp) && ($line_num < $end_line)) {
    $line = fgets($fp);
    $done = FALSE;

    if ($line_function_start == $line_num) {
      $function_name_head = _textmate_drush_function_name($line);
      $line = _textmate_drush_escape_line($line);
      $line = str_replace('function ' . $type . '_', 'function <?php print $basename; ?>_', $line);
      $done = TRUE;
    } else {
      if ($line_num == $start_escape && $start_escape == $end_escape) {
        $line = '  $1' . "\n" . '}';
        $done = TRUE;
      } elseif ($line_num == $start_escape && $start_escape + 1 == $end_escape) {
        $line = '  ${1:' . trim(_textmate_drush_escape_line($line)) . '}';
        $done = TRUE;
      } elseif ($line_num == $start_escape && $start_escape + 1 < $end_escape) {
        $line = '  ${1:' . trim(_textmate_drush_escape_line($line));
        $done = TRUE;
      }
    }

    if ($done == FALSE) {
      if ($line_num == $end_escape - 1) {
        $line = _textmate_drush_escape_line(rtrim($line)) . '}';
      } elseif ($line_num != $end_line - 1) {
        $line = _textmate_drush_escape_line($line);
      }
    }

    $body .= rtrim($line) . "\n";
    $_line++;
    $line_num++;
  }

  $head = '/**' . "\n";
  $head .= ' * Implements ' . $function_name_head . '().' . "\n";
  $head .= ' */' . "\n";
  $function = $head . $body . "\n" . '$2';

  return array('name' => $function_name_head, 'code' => rtrim($function));
}

/**
 * Print the specified function
 */
function _textmate_drush_print_function_name($function_name, $file, $start_line, $end_line) {
  $line_num = 0;
  $doxygen = NULL;
  $fp = fopen($file, 'r');

  $function = '';

  while (!feof($fp) && ($line_num < ($start_line - 1))) {
    $line = fgets($fp);
    ++$line_num;

    if (substr($line,0,3) == '/**') {
      $doxygen = $line;
    }
    elseif (isset($doxygen)) {
      $doxygen .= $line;
      if ($line_num + 1 == $start_line) {
        // drush_print(rtrim($doxygen));
      }
      if (strstr($line, '*/') !== FALSE) {
        $doxygen = NULL;
      }
    }
  }

  $line_function_start = $line_num;
  $start_escape = $line_num + 1;
  $end_escape = $end_line - 1;

  $escape = FALSE;
  while (!feof($fp) && ($line_num < $end_line)) {
    $line = fgets($fp);
    $done = FALSE;
    if ($line_function_start == $line_num) {
      return _textmate_drush_function_name($line);
    }
  }
}

function _textmate_drush_escape_line($line) {
  $line = str_replace('$', '\$', $line);
  $line = str_replace('}', '\}', $line);
  return $line;
}

/**
 * Filter hook functions
 **/
function _textmate_drush_hook_filter($function) {
  return substr($function, 0, 5) == 'hook_';
}

/**
 * Filter theme functions
 **/
function _textmate_drush_theme_filter($function) {
  static $theme;
  if (empty($theme)) {
    foreach (theme_get_registry() as $theme_function => $info) {
      $theme[] = 'theme_' . $theme_function;
    }
  }
  return in_array($function, $theme);
}

/**
 * Get function name from line
 **/
function _textmate_drush_function_name($line) {
  $line = explode(' ', $line);
  $function = explode('(', $line[1]);
  return $function[0];
}

function _textmate_drush_filter_files() {
  return array('.', '..', 'CVS', '.svn', '.git');
}
