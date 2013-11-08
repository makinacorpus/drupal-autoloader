# Autoloader

Drupal PSR-0 autoload helper for modules.

## User setup

### Quick setup

Nothing to do.

### Advanced setup (setting.php)

If you need pre-bootstrap classes to be loaded (cache backend is probably
the only use case of this) put this line into your settings.php file:

    include_once '[path/to]/vendor/autoload.php';

Where *[path/to]* to autoload file is the content of the 'autoloader_file_path'
variable.

Default location of the file is (if it could be generated):

    include_once 'sites/default/vendor/autoload.php';

Which falls back into a temporary (writable) folder if the module cannot write
into this file. Use the drush command to regenerate the autoload file whenever
possible in order to ensure the file can be write at the right place.

Notice that you can put it pretty much elsewhere, for example:

    $conf['autoloader_file_path'] = '/some/where/else';
    include_once '/some/where/else/vendor/autoload.php';

Note that this would be definitely a good idea to get this file way out of
public webroot and isolated into its own folder.

### Using Composer as autoloader

Composer can be used as class map generator and class loader for optimisation
purposes. While it's not mandatory since the module ships the sample
*SplClassLoader* from the PHP-FIG group it remains highly recommended to prefer
Composer's one.

Go to the generated *composer.json* path (as explained upper) and type in:

    cd sites/default
    php composer.phar install
    php composer.phar dump-autoload --optimize

## Module developer

### Basic usage

If you are a module developer and want to register a PSR-0 namespace just add
the following into your .info file:

    autoload[psr-0][MyNamespace] = lib

Where *lib* is the path of your namepace relative to the module folder.
Those values are supposed to be compatible with composer.json files
*autoload* declaration.

You are allowed to put as many namespaces as you wish there.

### Advanced composer usage

If your module depends on an external library based on composer add a
*composer.json* file into your module decribing your requirements. If such
file is found, project dependencies will be merge into the generated
*composer.json* file.

For refreshing your project dependencies run:

    drush build-autoload
    cd sites/default
    php composer.phar update
    php composer.phar dump-autoload --optimize 

Everything should be up and running.

