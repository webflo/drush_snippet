<?php

interface SnippetGeneratorInterface {
  public function __construct($type, $version, $settings = array());

  public function find();

  public function parse();

  public function format($type, $file, $start_line, $end_line);

  public function write($src, $functions);
}

class SnippetGenerator implements SnippetGeneratorInterface {
  public $type = NULL;

  public $version = NULL;

  public $settings = array();

  public function __construct($type, $version, $settings = array()) {
    $this->type = $type;
    $this->version = $version;
    $this->settings += $settings;
  }

  public function find() {
    $scan = array();

    switch ($this->type) {
      case 'hook':
        switch ($this->version) {
          case 7:
            $scan = $scan + drush_scan_directory('./modules', '/\.api\.php$/', $this->filter_files());
            break;

          case 6:
            $scan = $scan + drush_scan_directory('./docs', '/\.php$/', $this->filter_files());
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
        $scan = $scan + drush_scan_directory('./modules', '/\.inc$/', $this->filter_files());
        $scan = $scan + drush_scan_directory('./modules', '/\.module$/', $this->filter_files());

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
    $functions = array_values(array_filter($functions['user'], array($this, "filter_{$this->type}_function")));
    sort($functions);
    $this->functions = $functions;
  }

  function parse() {
    $functions = array();

    foreach($this->functions as $function_name) {
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

      $function = $this->format($this->type, $reflect->getFileName(), $reflect->getStartLine(), $reflect->getEndLine());
      $functions[$function_name] = $function;
      if (isset($write_files)) {
        $this->write($function);
      } else {
        // drush_print_r($function);
      }
    }
    return $functions;
  }

  /**
   * Filter include files.
   *
   * @return array
   */
  public function filter_files() {
    return array('.', '..', 'CVS', '.svn', '.git');
  }

  /**
   * Filter hooks functions.
   *
   * @param $function
   *
   * @return bool
   */
  public function filter_hook_function($function) {
    return substr($function, 0, 5) == 'hook_';
  }

  /**
   * Filter theme functions.
   *
   * @param $function
   *
   * @return bool
   */
  public function filter_theme_function($function) {
    static $theme;
    if (empty($theme)) {
      foreach (theme_get_registry() as $theme_function => $info) {
        $theme[] = 'theme_' . $theme_function;
      }
    }
    return in_array($function, $theme);
  }

  public function format($type, $file, $start_line, $end_line) {}
  public function write($src, $functions) {}

  /**
   * Get function name string/line.
   **/
  function extract_function_name($line) {
    $line = explode(' ', $line);
    $function = explode('(', $line[1]);
    return $function[0];
  }
  function escape_line($line) {
    return $line;
  }
}

class SnippetGeneratorTextmate extends SnippetGenerator {
  function escape_line($line) {
    return str_replace(array('$', '}'), array('\$', '\}'), $line);
  }
}

class SnippetGeneratorPhpStorm extends SnippetGenerator {
  function escape_line($line) {
    return htmlentities($line);
  }

  function write($src = NULL, $functions) {
    $xml = '<?xml version="1.0" encoding="UTF-8"?><templateSet group="Drupal 7">';
    foreach($functions as $function) {
      $snippet = htmlspecialchars($function['code'], ENT_QUOTES);
      $snippet = str_replace(array("\n"), array('&#10;'), $snippet);
      $xml .= '
      <template name="' . $function['name'] . '" value="' . $snippet . '" description="" toReformat="false" toShortenFQNames="true">
         <variable name="COMMENT" expression="" defaultValue="" alwaysStopAt="true" />
         <variable name="MODULE_NAME" expression="" defaultValue="" alwaysStopAt="true" />
         <context>
           <option name="PHP" value="true" />
         </context>
       </template>
       ';
    }
    $xml .= '</templateSet>';
    if (empty($src)) {
      drush_print_r($xml);
    } else {
      file_put_contents($src, $xml);
    }
  }

    function format($type, $file, $start_line, $end_line) {
      $line_num = 0;
      $doxygen = NULL;
      $fp = fopen($file, 'r');

      $function = '';
      $body = '';

      while (!feof($fp) && ($line_num < ($start_line - 1))) {
        $line = fgets($fp);
        ++$line_num;

        if (substr($line,0,3) == '/**') {
          $doxygen = $line;
        }
        elseif (isset($doxygen)) {
          $doxygen .= $line;
          if ($line_num + 1 == $start_line) {
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
          $function_name_head = $this->extract_function_name($line);
          // $line = $this->escape_line($line);
          /*
           * $line = str_replace('function ' . $type . '_', 'function <?php print $basename; ?>_', $line);
           */
          $done = TRUE;
        } else {
          if ($line_num == $start_escape && $start_escape == $end_escape) {
            // $line = '  $1' . "\n" . '}';
            $done = TRUE;
          } elseif ($line_num == $start_escape && $start_escape + 1 == $end_escape) {
            // $line = '  ${1:' . trim($this->escape_line($line)) . '}';
            $done = TRUE;
          } elseif ($line_num == $start_escape && $start_escape + 1 < $end_escape) {
            // $line = '  ${1:' . trim($this->escape_line($line));
            $done = TRUE;
          }
        }

        /*
        if ($done == FALSE) {
          if ($line_num == $end_escape - 1) {
            $line = $this->escape_line(rtrim($line)) . '}';
          } elseif ($line_num != $end_line - 1) {
            $line = $this->escape_line($line);
          }
        }
        */

        $body .= rtrim($line) . "\n";
        $line_num++;
      }

      $head = '/**' . "\n";
      $head .= ' * Implements ' . $function_name_head . '().' . "\n";
      $head .= ' */' . "\n";
      $function = $head . $body . "\n";

      return array('name' => $function_name_head, 'code' => rtrim($function));
    }
}
