# Drush Snippet Generator

## Installation

git clone git://github.com/webflo/drush_snippet.git ~/.drush/drush_snippet

## Usage

Go to an existing Drupal installation and execute the following command.

    // Hooks for PhpStrom
    drush snippet-generate --type=hook --output=/tmp/d7_hooks.xml --format=PhpStorm

    // Theme functions for PhpStrom
    drush snippet-generate --type=theme --output=/tmp/d7_theme.xml --format=PhpStorm
