# BeamerPresenter
This project aims at creating a simple PDF viewer, which shows one PDF file as a
presentation in full screen mode and another PDF file with notes for the slides
on a second screen.

BeamerPresenter is only tested with presentations and notes generated by LaTeX
beamer. In contrast to other presentation programs, which use the beamer option
`show notes on second screen`, this program can show note pages, which have a
different size and aspect ratio than the slides.
This can be useful for slides with aspect ratio 16:9 or even wider.

Support for presentations generated with the beamer option
`show notes on second screen` is also present, but the focus of this program
lies on showing two independent PDFs. My personal impression is that LaTeX
beamer sometimes behaves unexpectedly in this mode.


## Build
To build and use this project you need to have the Qt5 multimedia module and the
poppler Qt5 bindings installed.

Download an compile this project:
```sh
git clone https://github.com/stiglers-eponym/BeamerPresenter.git
cd BeamerPresenter
qmake && make
```


## Usage
```sh
beamerpresenter <presentation.pdf> <notes.pdf>
```
For more options and usage possibilities use `beamerpresenter --help`.


## Settings
Settings can be placed in a file `beamerpresenter.conf` (on platforms other than
Linux: `beamerpresenter.ini`). An example configuration file is provided.
The configuration can only be edited directly with a text editor.

This program is only tested on a Linux system. Using configuration files might
lead to platform specific problems.


## Patching LaTeX beamer
The typical usage case requires notes with the same page number as the
presentation. Generating such notes in LaTeX beamer can be achieved by applying
the following patch:

```tex
\setbeameroption{show only notes}
\makeatletter
  %
  % Overwrite beamer's definition of \beamer@framenotesend, such that for each
  % page generated in the presentation, one page will be generated in the notes.
  %
  \def\beamer@framenotesend
  {% at end of slide
    \global\setbox\beamer@frameboxcopy
      =\hbox{\leaders\copy\beamer@framebox\hskip\wd\beamer@framebox}
    \beamer@setupnote%
    \ifbeamer@notesnormals%
    \else%
      \global\setbox\beamer@framebox=\box\voidb@x%
    \fi%
  }
\makeatother
```

Alternatively you can use beamer's option to combine presentation and notes:
```tex
\usepackage{pgfpages}
\setbeameroption{show notes on second screen=right}
```
In this case links on the notes pages might require some patches in LaTeX.


## Embed Applications
You can make external programs fit into your presetation:
Call the program from a link in the PDF and embed the window of the program in
the link area inside the PDF. This requires that beamerpresenter knows the
window ID of your program. It can either be given in the standard output of the
program or by using an external application, which takes the process ID as an
argument and returns the window ID.

An example of a shell script using `wmctrl` for this purpose is:
```sh
#!/bin/bash
echo $(( 16#$(wmctrl -lp | sed -n "s/^0x\([0-9a-f]\+\) \+[0-9]\+ \+$1 .*$/\1/p") ))
```
