# Tattle wx

**This is a work in progress and may still be subject to compatibility-breaking changes.**

Tattle is a native utility for purposes of error reporting, feedback and data collection.  It is meant to be invoked by another application, compiles a report consisting of command-line arguments, attached files, and user-entered data, and uploads it to an HTTP server using the POST method.

Tattle's behavior is defined by arguments which may come from a command line or configuration file.  It is designed as an executable rather than a library; this isolates it from critical errors in the calling application.


## Workflow

Tattle is generally invoked by another application, which provides it with a configuration.  Tattle compiles a list of parameters, which can include text and file data as well as fields for the user to fill in.  Before asking the user for information, Tattle can make a GET request to the server in order to identify a problem and suggest a solution.  Assuming that does not prevent 

Step-by-step:

1. Parse Configuration (see below)
  * Includes the command-line and any configuration files.
  * This builds the parameter list.
2. Read files into memory
  * This is done immediately.  (in case files change)
  * File truncation is applied here, limiting memory usage.
3. Advance Query _(`-uq` enables)_
  * Fire HTTP GET request, with `-aq` parameters included in the URL query.
  * If the server responds with text or a link, this is displayed to the user.
  * _The server may specify that execution should stop here.  (Not yet implemented!)_
  * This step happens only if the `-uq` flag is passed.
4. Prompt _(`-s` bypasses)_
  * Display a prompt to the user, which may include an informative message and input fields.
  * The user may proceed using the **send** button, or abort using the **cancel** button.
  * The user may view the contents of the error report with the **view data** button.
5. Post
  * All parameters are encoded into an HTTP POST request and sent to the post URL.
  * If the post fails, the user is returned to the prompt.

Typically, Tattle displays a UI which will, at minimum, allow the user to either send the report or cancel it.  Any fields specified in the configuration will be displayed to the user, their contents submitted


## Configuration (command-line arguments)

Version 0.3 of the tattle command-line interface.  Note that parameters containing an `=` should be supplied as a single string.

| Flag  | Long Form        | Argument          | Effect |
|-------|------------------|-------------------|--------|
| `-h`  | `--help`         | _none_            | Displays help on command-line parameters. |
| `-up` | `--url-post`     | `<url>`           | **Required**.  URL for posting error reports. |
| `-uq` | `--url-query`    | `<url>`           | URL for advance query. |
| `-s`  | `--silent`       | _none_            | Post a report without displaying any UI. |
| `-c`  | `--config-file`  | `<fname>`         | Specify a configuration file containing more of these options. |
| `-a`  | `--arg`          | `<param>=<value>` | Specify argument string `<value>` for parameter `<param>`. |
| `-aq` | `--arg-query`    | `<param>=<value>` | As `-a` but also included in advance query. |
| `-ft` | `--file`         | `<param>=<fname>` | Upload a text file `<fname>` for parameter `<param>`. |
| `-fb` | `--file-binary`  | `<param>=<fname>` | Upload a binary file.  (Different content-type.) |
| `-tb` | `--trunc-begin`  | `<param>=<size>`  | Truncate a file parameter.  Include at least the first N bytes. |
| `-te` | `--trunc-end`    | `<param>=<size>`  | As `-tb` but preserve the last N bytes.  Combines with `-tb`! |
| `-tn` | `--trunc-note`   | `<param>=<text>`  | A line of text marking the truncation. |
| `-pt` | `--title`        | `<text>`          | A title for the prompt window. |
| `-pm` | `--message`      | `<text>`          | A message appearing at the top of the prompt window. |
| `-ps` | `--label-send`   | `<text>`          | Label for the 'Send Report' button. |
| `-pc` | `--label-cancel` | `<text>`          | Label for the 'Don't Send Report' button. |
| `-pv` | `--label-view`   | `<text>`          | Label for the 'View Data' button.  (Enable with `-v`) |
| `-i`  | `--field`        | `<param>=<label>` | Define a single-line field for `<param>`, with instructive label. |
| `-im` | `--field-multi`  | `<param>=<label>` | Define a multi-line field. |
| `-id` | `--field-default`| `<param>=<value>` | Define a default value for the field corresponding to `<param>`. |
| `-ih` | `--field-hint`   | `<param>=<hint>`  | Provide a hint message for an empty `-pf` parameter. |
| `-v`  | `--view-data`    | _none_            | Enable 'View Data' dialog so users can see report contents. |
| `-vd` | `--view-dir`     | `<path>`          | A folder linked from the 'View Data' dialog. |


## This implementation

**tattle_wx** is based on the wxWidgets framework, depending on its Core, Base and Net libraries.  This enables portability to Windows, Mac OS X and Linux (and potentially others).  It adheres to the C++98 standard.

The author is interested in alternative implementations of this utility, in particular native versions which minimize its footprint and methods of porting it to mobile.


## Building Notes

Under OSX/Clang with the latest wxWidgets it may be necessary to define FORCE_TR1_TYPE_TRAITS depending on the runtime libraries and flags used to compile wxWidgets and the applications itself.

When development is complete I plan to include precompiled binaries here.


## Development Notes

Tattle is inspired by the Unity crash reporter and Google Breakpad.  As far as I know it can be used as an uploader in a Breakpad-enabled app (but I haven't tried this yet)

Things that still need doing:

* Linking to the `-cd` folder.
* Verify full unicode compatibility.

Things I may investigate in the future:

* A progress bar for the report upload.
* Persistent fields (such as user's E-mail address)
* File compression.
* A way to save compiled reports to disk.
* Possibly some built in "auto" parameters such as a system description.
