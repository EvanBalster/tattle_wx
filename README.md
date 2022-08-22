# Tattle/wx

**This is a work in progress and may still be subject to compatibility-breaking changes.**

Tattle is a native utility for purposes of error reporting, software update notifications, feedback and data collection.  It is meant to be invoked by another application, compiles a report consisting of command-line arguments, attached files, and user-entered data, and uploads it to an HTTP server using the POST method.

Tattle's behavior is defined by arguments which may come from a command line or configuration file.  It is designed as an executable rather than a library; this isolates it from critical errors in the calling application.


## Workflow

Tattle is generally invoked by another application, which provides it with a configuration.  Tattle compiles a list of parameters, which can include text and file data as well as fields for the user to fill in.  Before asking the user for information, Tattle can make a GET request to the server in order to identify a problem and suggest a solution.  Assuming that does not prevent 

Step-by-step:

1. Parse Configuration (see below)
  * Includes the command-line and any configuration files.
  * This builds the parameter list.
  * Files are truncated and read into memory at this stage, in case they would subsequently change.
2. ❌ Planned: Consent to Query *(`-s` or `-sq` bypasses)*
   * ❌ Ask the user for consent to send basic information to the server.
   * This prompt is only shown to the user once per category.
3. Query _(`-uq` enables)_
  * Fire HTTP GET request, with `-aq` parameters encoded as multipart POST data.
  * If the server responds with text or a link, this is displayed to the user.
  * _The server may specify that execution should stop here.  (Not yet implemented!)_
  * This step happens only if the `-uq` flag is passed.
4. Prompt + Consent to Post _(`-s` bypasses)_
  * Display a prompt to the user, which may include an informative message and input fields.
  * The user may proceed using the **send** button, or abort using the **cancel** button.
  * The user may view the contents of the error report with the **view data** button.
5. Post _(`-up` enables)_
  * All parameters are encoded into an HTTP POST request and sent to the post URL.
  * If the post fails, the user is returned to the prompt.

Typically, Tattle displays a UI which will, at minimum, allow the user to either send the report or cancel it.  Any fields specified in the configuration will be displayed to the user, their contents submitted when the user chooses to send the report.


## Configuration (command-line arguments)

Version 0.5 of the tattle command-line interface.  Note that parameters containing an `=` should be supplied as a single string.

Items marked with ❌ are not yet implemented.

| Abbr  | Flag/Option               | Argument          | Effect                                                       |
| ----- | ------------------------- | ----------------- | ------------------------------------------------------------ |
| `-h`  | `--help`                  | _none_            | Displays help on command-line parameters.                    |
| `-c`  | `--config-file`           | `<fname>`         | Supply additional arguments using a file.<br />Helps avoid command-line limits on Windows. |
|       | <u>**Service URLs**</u>   |                   | *at least one URL is required; it may be placed in a config file.* |
| `-uq` | `--url-query`             | `<url>`           | URL for initial server query.                                |
| `-up` | `--url-post`              | `<url>`           | URL for posting error reports.                               |
|       | **<u>Data & Privacy</u>** |                   |                                                              |
| `-v`  | `--view-data`             |                   | Enable user to view content from the prompt.                 |
| `-vd` | `--view-dir`              | `<path>`          | A folder linked from the 'View Data' dialog.<br />This option is taken to imply `-v`. |
| `-cs` | `--cookies`               | `<path>`          | ❌ Directory for Tattle's persistent data.<br />Enables persistent inputs, consent & server cookies. |
| `-ct` | `--category`              | `<type>:<id>`     | Identifies report for features like "don't show again".<br />Enables "don't show again" feature |
| `-l`  | `--log`                   | `<fname>`         | Set Tattle's log file location (default: no logging).        |
| `-sq` | `--silent-query`          |                   | Query without requiring user consent.                        |
| `-s`  | `--silent`                |                   | Query and post without displaying any UI.<br />⚠ **Bypasses all user consent**. |
|       | <u>**Data Content**</u>   |                   |                                                              |
|       |                           |                   |                                                              |
| `-a`  | `--arg`                   | `<key>=<value>`   | Specify argument string `<value>` for parameter `<key>`.     |
| `-aq` | `--arg-query`             | `<key>=<value>`   | As `-a` but also included in initial query.                  |
| `-ai` | `--arg-id`                | `<key>=<value>`   | ❌ Identity within this operation's category.<br />Used for tracking "don't show me this again".<br />Otherwise behaves as `--arg-query`. |
| `-ft` | `--file`                  | `<key>=<fname>`   | Upload a text file `<fname>` for parameter `<key>`.          |
| `-fb` | `--file-binary`           | `<key>=<fname>`   | Upload a binary file.  (Different content-type.)             |
| `-tb` | `--trunc-begin`           | `<key>=<size>`    | Truncate a file parameter.<br />Includes at least the first N bytes. |
| `-te` | `--trunc-end`             | `<key>=<size>`    | As `-tb` but preserve the last N bytes.<br />Combines with `-tb`! |
| `-tn` | `--trunc-note`            | `<key>=<text>`    | A line of text inserted at the truncation.                   |
|       | <u>**Prompt Config**</u>  |                   |                                                              |
| `-pt` | `--title`                 | `<text>`          | A title for the prompt window.                               |
| `-pm` | `--message`               | `<text>`          | A message appearing at the top of the prompt window.         |
| `-px` | `--technical`             | `<text>`          | Technical info appearing in the prompt window.               |
| `-wi` | `--icon`                  | `<system icon>`   | May be `info`, `error`, `warn`, `question`, `help` or `tip`. |
| `-wp` | `--show-progress`         |                   | Enable progress bars; useful for large uploads.              |
| `-wt` | `--stay-on-top`           |                   | Tattle's windows stay on top of all others.                  |
| `-ps` | `--label-send`            | `<text>`          | Label for the 'Send Report' button.                          |
| `-pc` | `--label-cancel`          | `<text>`          | Label for the 'Don't Send Report' button.                    |
| `-pv` | `--label-view`            | `<text>`          | Label for the 'View Data' button.  (Enable with `-v`)        |
|       | **<u>Prompt Inputs</u>**  |                   |                                                              |
| `-i`  | `--field`                 | `<key>=<label>`   | Define a single-line field with instructive label.           |
| `-im` | `--field-multi`           | `<key>=<label>`   | Define a multi-line field for `param`.                       |
| `-is` | `--field-store`           | `<key>`           | Save `<key>` for re-use in future prompts.<br />Requires data directory specified with `-d`. |
| `-id` | `--field-default`         | `<key>=<value>`   | Define a default value for `<key>`.                          |
| `-ih` | `--field-hint`            | `<key>=<hint>`    | Provide a hint message for empty `-i` fields.                |
| `-iw` | `--field-wanted`          | `<key>=<nessage>` | ❌ Prompt the user for confirmation if a field is left empty. |

## This implementation

**tattle_wx** is based on the wxWidgets framework.  This enables portability to Windows, Mac OS X and Linux (and potentially others).  It adheres to the C++11 standard.

The author is interested in alternative implementations of this utility, in particular OS-native versions (which could minimize its footprint) and mobile versions.


## Building Tattle/wx

It should be possible to build Tattle with wxWidgets 2 or 3 and any C++98-compatible compiler, but I have not tested extensively.  It depends on wxWidgets' **core**, **base** and **net** modules.

Under OSX/Clang with the latest wxWidgets it may be necessary to define FORCE_TR1_TYPE_TRAITS depending on the runtime libraries and flags used to compile wxWidgets and the applications itself.

When development is complete I plan to include precompiled binaries here.


## Development Notes

Tattle is inspired by the Unity crash reporter, Mozilla crash reporter and Google Breakpad.  As far as I know it can be used as an uploader in a Breakpad-enabled app (but I haven't tried this yet)

Things that still need doing:

* Verify unicode compatibility.

Things I may investigate in the future:

* More graceful parent/child window behavior with the spawning process.
* Nicer format for Tattle's configuration (such as XML)
* Language files for localizing the UI
* Persistent fields (such as user's E-mail address)
* GZIP-compressed uploading.
* A way to save compiled reports to disk.
* Possibly some built in "automatic" parameters
  * System description
  * Report time (local/GMT)
