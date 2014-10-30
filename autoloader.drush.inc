<?php

/**
 * @file
 * Autoloader Drush commands.
 */

/**
 * Implements hook_drush_command().
 */
function autoloader_drush_command() {
  return array(
    'build-autoload' => array(
      'description' => '(Re)build vendor/autoload.php and composer.json files.',
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    ),
  );
}

/**
 * (Re)build vendor/autoload.php and composer.json files.
 */
function drush_autoloader_build_autoload() {
  require_once __DIR__ . '/autoloader.refresh.inc';
  $path = autoloader_refresh_real(false, function ($message, $args = array()) {
    drush_print(dt($message, $args));
  });
  if (!$path) {
    drush_print(dt("Unable to write file"), 'error');
    return 1;
  }
  drush_print(dt("File wrote to @file", array('@file' => $path)));
  drush_print(dt("You can now run: @composer", array('@composer' => "\n    cd " . dirname($path) . "\n    php composer.phar dump-autoload --optimize")));
}
