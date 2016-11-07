# Tattle wx

** This is a work in progress and may not be viable for serious usage yet. **

Tattle is a native utlity for purposes of error reporting, feedback and data collection.  It is meant to be invoked by another application, and compiles a report consisting of string fields, attached files, and user-entered data, and uploads it to an HTTP server using the POST method.

Tattle's behavior is defined by arguments which may come from a command line or configuration file.  It is designed as an executable rather than a library; this isolates it from critical errors in the calling application.


## Workflow

When invoked, Tattle parses its configuration (see below) and fills its parameter list with a combination of application-supplied values, user fields and attached files.  A target URL is required.

Typically, Tattle displays a UI which will, at minimum, allow the user to either send the report or cancel it.  Any fields specified in the configuration will be displayed to the user, their contents submitted


## Configuration (command-line arguments)

Version 0.1 of the tattle command-line interface.  Note that parameters containing an `=` should be supplied as a single string.

| Flag  | Long Form        | Argument          | Effect |
|-------|------------------|-------------------|--------|
| `-h`  | `--help`         | _none_            | Displays help on command-line parameters. |
| `-u`  | `--url`          | `<url>`           | **Required**.  Specify the desination URL. |
| `-s`  | `--silent`       | _none_            | Post a report without showing any UI.  No user fields will be filled. |
| `-c`  | `--config-file`  | `<fname>`         | Specify a configuration file containing more command-line arguments. |
| `-a`  | `--arg`          | `<param>=<value>` | Specify argument string `<value>` for parameter `<param>`. |
| `-ft` | `--file`         | `<param>=<fname>` | Upload a text file `<fname>` for parameter `<param>`. |
| `-fb` | `--file-binary`  | `<param>=<fname>` | Upload a binary file.  (Linked rather than printed in the UI) |
| `-pi` | `--title`        | `<text>`          | Supply a title for the UI window. |
| `-pm` | `--message`      | `<text>`          | Supply a message which will appear as the header of the UI. |
| `-pf` | `--field`        | `<param>=<label>` | Define a single-line field for `<param>`, with an instructive label. |
| `-pm` | `--field-multi`  | `<param>=<label>` | Define a multi-line field. |
| `-pd` | `--field-default`| `<param>=<value>` | Define a default value for the field corresponding to `<param>`. |
| `-ph` | `--field-hint`   | `<param>=<hint>`  | Provide a hint message for a parameter.  Will appear in empty `-pf` fields. |
| `-cd` | `--content-dir`  | `<label>=<path>`  | Supply a folder which will be linked in the "view contents" dialog. |


## This implementation

**tattle_wx** is based on the wxWidgets framework, depending on its Core, Base and Net libraries.  This enables portability to Windows, Mac OS X and Linux (and potentially others).  It adheres to the C++98 standard.

The author is interested in alternative implementations of this utility, in particular native versions which minimize its filesize and implementations which might make it useful on mobile.


## Development Notes

Tattle is inspired by the Unity crash reporter and Google Breakpad.  I believe it can be used as an uploader in a Breakpad-enabled app.
