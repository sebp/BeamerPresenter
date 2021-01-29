# BeamerPresenter
BeamerPresenter is a PDF viewer for presentations, which opens a presentation
screen and a control screen in two different windows. The control screen
optionally shows slides from a dedicated notes document instead of the slides
for the audience. Additional information on the control screen includes the
current time, a timer for the presentation, and previews of the next slides.

This software uses the Qt framework and the PDF engines MuPDF and/or poppler.

## Features
* modular user interface (new in 0.2.x)
* render slides to compressed cache for fast response
* poppler or MuPDF (new in 0.2.x) as PDF engine
* draw in slides (improved in 0.2.x)
* notes for the speaker in Markdown format (new in 0.2.x)
* optionally to show separate file for speaker
* optionally use LaTeX-beamer's option to show notes on second screen and split PDF pages into a part for the speaker and a part or the audience
* clock and timer for the presentation (some features of 0.1.x not yet available in 0.2.x)
* simple navigation using document outline, thumbnail slides, or navigation by page label

### Currently NOT supported in version 0.2.x
* highlighting tools (torch, magnifier, pointer) (only 0.1.x)
* multimedia content on slides (only 0.1.x)
* slide transitions (only 0.1.x)
* save/load drawings (only 0.1.x)


## Build
Building is tested in an up-to-date Arch Linux and (from time to time) in ubuntu 20.04.
Older versions of ubuntu are not supported, because ubuntu 18.04 uses old versions of poppler and MuPDF and other versions before 20.04 should not be used anymore anyhow.
Version 0.1.x of BeamerPresenter should support ubuntu 18.04 and you should create an issue on github if it does not.

Install required packages. You need Qt5 including the multimedia module.
Additionally you need either the Qt5 bindings of poppler or the MuPDF C bindings.

Poppler dependencies in ubuntu:
    * `libpoppler-qt5-dev` version 21.01 is tested. Versions below 0.70 are explicitly not supported, compiler errors in newer versions might be fixed if reported in an issue on github.

MuPDF dependencies:
    * `libmupdf-dev` MuPDF versions starting from 1.17 should work, version 1.12 or older is explicitly not supported.
    * `libfreetype-dev`
    * `libharfbuzz-dev`
    * `libgumbo-dev` (for MuPDF 1.18, probably not in 1.17)
    * `libjpeg-dev`
    * `libjbig2dec0-dev`
    * `libopenjp2-7-dev`

First download the sources
```sh
git clone --depth 1 git@github.com:stiglers-eponym/BeamerPresenter.git
```
Now you need to select the PDF engine. In the file `beamerpresenter.pro`
you will find the lines
`DEFINES += INCLUDE_POPPLER` and
`DEFINES += INCLUDE_MUPDF`.
Comment out the PDF engine which you don't need with a `#`.

Now you can start building.
```sh
qmake && make
make install
```
If this fails, check your Qt version (`qmake --version`).
If you use 5.8 < qt < 6, you should open an issue on github. In older versions
you may also open an issue, but it will probably not be fixed.

## Development
Version 0.2.x of BeamerPresenter has been developed independent of version 0.1.y
with the aim of avoiding the chaotic old code.
The configuration files of version 0.2.x and 0.1.x are incompatible.

#### Already implemented
* render with Poppler or MuPDF
* cache pages (with limitation of available memory; doesn't work if pages have different sizes)
* read slides and notes from the same PDF, side by side on same page
* navigation links inside document
* navigation skipping overlays
* build flexible GUI from config (not really stable)
* draw and erase using tablet input device with variable pressure (wacom)
* full per-slide history of drawings (with limitation of number of history steps)
* widgets:
    * slide
    * clock
    * page number
    * page label (and max. label)
    * tool selector
    * timer
    * editable markdown notes per page label
    * table of contents (still kind of improvised)
    * thumbnails (still kind of improvised)

#### To be implemented
* improve cache management and layout corrections: sometimes cache is not used correctly.
* avoid fatal error when stopping program while cache threads are running
* cache slides even when size of slides varies
* cache only required slides in previews showing specific overlays
* fix layout, avoid recalculating full layout when clock label changes text
* immediately update slides to fit window size (currently this is only done when changing the slide or manually updating)
* animations and automatic slide change
* slide transitions
* multimedia
* option to insert extra slides or extra space below slide for drawing
* switch between per-slide and per-overlay drawings
* highlighting tools: pointer, torch, magnifier
* save and load drawings to xopp-like xml format
* improve widgets:
    * settings (GUI for config files, experimental version already available)
    * timer (with color indicating progress relative to estimate like in version 0.1.x)
    * thumbnails (cursor)
    * table of contents (kind ov improvised)
    * all: keyboard shortcuts
* fine-tuned interface, fonts, ...
* manual, man pages


## License
This software may be redistributed and/or modified under the terms of the GNU Affero General Public License (AGPL), version 3, available on the [GNU web site](https://www.gnu.org/licenses/agpl-3.0.html). This license is chosen in order to ensure compatibility with the software libraries used by BeamerPresenter, including Qt, MuPDF, and poppler.

BeamerPresenter can be compiled without including MuPDF, when using only poppler as a PDF engine.
Those parts of the software which can be used without linking to MuPDF may, alternatively to the AGPL, be redistributed and/or modified under the terms of the GNU General Public License (GPL), version 3 or any later version, available on the [GNU web site](https://www.gnu.org/licenses/gpl-3.0.html).

BeamerPresenter is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

This is an early development version. It is NOT READY FOR USAGE.
