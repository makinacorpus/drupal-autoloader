<?php

/**
 * Pretty print JSON.
 */
if (!defined('JSON_PRETTY_PRINT')) {
  define('JSON_PRETTY_PRINT', 0);
}

/**
 * Don't escape '/' in JSON.
 */
if (!defined('JSON_UNESCAPED_SLASHES')) {
  define('JSON_UNESCAPED_SLASHES', 0);
}

/**
 * Load module file if necessary.
 */
if (!defined('AUTOLOADER_COMPOSER_ENABLE')) {
  require_once __DIR__ . '/autoloader.module';
}

/**
 * Build a classmap for loading core classes.
 */
function autoloader_refresh_core_loader() {

  $map = Database::getConnection('default', 'default')->query("SELECT name, filename FROM {registry}")->fetchAllKeyed();

  if (empty($map)) {
    return '';
  }

  $data  = "\n\$__autoloaderCoreMap = array(\n";
  foreach ($map as $symbol => $path) {
    $data .= "    '$symbol' => DRUPAL_ROOT . '/$path',\n";
  }
  $data .= "\n);\n";
  $data .= "\nspl_autoload_register(function (\$className) use (\$__autoloaderCoreMap) {";
  $data .= "\n  if (isset(\$__autoloaderCoreMap[\$className])) {";
  $data .= "\n    return @include_once \$__autoloaderCoreMap[\$className];";
  $data .= "\n  }";
  $data .= "\n}, null, true);\n";

  return $data;
}

/**
 * @see autoloader_refresh().
 */
function autoloader_refresh_real($register = false, $log = null) {

  /***************************************************************************
   *
   * Prepare some bits of environment.
   *
   ***************************************************************************/

  if (null === $log || !is_callable($log)) {
    $log = function ($message, $args = array()) {
      watchdog('autoloader', $message, $args, WATCHDOG_ERROR);
    };
  }

  if ($register) {
    require_once __DIR__ . '/resources/SplClassLoader.php';
  }

  /***************************************************************************
   *
   * Ensure file path.
   *
   ***************************************************************************/

  $where  = autoloader_file_path();
  $prefix = str_repeat('../', substr_count($where, '/') + 1);

  /***************************************************************************
   *
   * Find Drupal modules custom namespaces.
   *
   ***************************************************************************/

  $map = array();
  $cmap = array();

  foreach (autoloader_find_modules() as $path => $methods) {
    foreach ($methods as $method => $namespaces) {
      foreach ($namespaces as $namespace => $relpath) {
        $map[$method][$namespace] = $path . '/' . $relpath;
        $cmap[$method][$namespace] = $prefix . $path . '/' . $relpath;

        if ($register) {
          $loader = new SplClassLoader(
            $namespace,
            DRUPAL_ROOT . '/' . $path . '/' . $relpath
          );
          $loader->register();
        }
      }
    }
  }

  /***************************************************************************
   *
   * Merge composer dependencies.
   * @todo This probably needs to be a bit more robust.
   *
   ***************************************************************************/

  $dependencies = array();

  if (variable_get(AUTOLOADER_COMPOSER_ENABLE, true)) {
    foreach (autoloader_find_composer_dependencies() as $file) {

      if (!$data = file_get_contents($file)) {
        call_user_func($log, "Could not read file @file", array('@file' => $file));
        continue;
      }

      if (!$data = json_decode($data, true)) {
        call_user_func($log, "Invalid JSON in file @file", array('@file' => $file));
        continue;
      }

      if (empty($data['require']) || !is_array($data['require'])) {
        call_user_func($log, "No dependencies found in file @file", array('@file' => $file));
        continue;
      }

      $dependencies += $data['require'];
    }
  }

  /***************************************************************************
   *
   * Build composer file.
   *
   ***************************************************************************/

  $composer = array(
    'name'        => 'drupal-site/' . variable_get('install_profile', "custom"),
    'type'        => 'drupal-site',
    'description' => sprintf("Automatically generated file for site '%s'.", variable_get('site_name', "Drupal")),
    'autoload'    => $cmap,
  );

  if (!empty($dependencies)) {
    $composer['require'] = $dependencies;
  }

  $data = json_encode($composer, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);

  if (!file_put_contents($where . '/composer.json', $data)) {
    call_user_func($log, "Could not write file @file", array('@file' => $where . '/composer.json'));
  }

  /***************************************************************************
   *
   * Build custom temporary autoload.php file.
   *
   ***************************************************************************/

  $vendor = $where . '/vendor';
  if (!is_dir($vendor)) {
    mkdir($vendor);
  }
  if (!is_writable($vendor)) {
    call_user_func($log, "Directory is not writable @dir", array('@dir' => $vendor));
    return false;
  }

  // Generate working autoload file without composer using a stupid yet
  // simple SPL autoloader.
  if (!function_exists('drupal_get_path')) {
    require_once DRUPAL_ROOT . '/includes/common.inc';
  }
  $loaderpath = drupal_get_path('module', 'autoloader') . '/resources/SplClassLoader.php';
  $data = "<?php\n\nif (!include_once(DRUPAL_ROOT . '/$loaderpath')) {\n return false;\n}";

  if (!empty($map['psr-0'])) {
    foreach ($map['psr-0'] as $namespace => $path) {
      $data .= "\n\n\$loader = new SplClassLoader('$namespace', DRUPAL_ROOT . '/$path');";
      // Tiny fix that will ensure the prefix separator for PEAR style
      // libraries if they are declared the same way as it is documented
      // at http://getcomposer.org/doc/04-schema.md#autoload
      if ('_' === $namespace[strlen($namespace) - 1]) {
        $data .= "\n\$loader->setNamespaceSeparator('_');";
      }
      $data .= "\n\$loader->register();";
    }
  }
  $data .= "\n";

  // Generate a classmap for core if necessary.
  if (variable_get('autoloader_core_cache', true)) {
    $data .= autoloader_refresh_core_loader();
  }

  $autoloadfile = $vendor . '/autoload.php';
  if (!file_put_contents($autoloadfile, $data)) {
    call_user_func($log, "Could not write file @file", array('@file' => $autoloadfile));
    return false;
  }

  return $autoloadfile;
}

/**
 * Find modules using the "autoload" info variable.
 */
function autoloader_find_modules() {
  $ret = array();
  foreach (system_list('module_enabled') as $info) {
    if (!empty($info->info['autoload'])) {
      $ret[dirname($info->filename)] = $info->info['autoload'];
    }
  }
  return $ret;
}

/**
 * Find modules having a composer.json file describing dependencies.
 */
function autoloader_find_composer_dependencies() {
  $ret = array();
  foreach (system_list('module_enabled') as $module => $info) {
    $path = dirname($info->filename) . '/composer.json';
    if (file_exists($path)) {
      $ret[$module] = $path;
    }
  }
  return $ret;
}
