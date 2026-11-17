# K-MIDI

K-MIDI makes **MIDI templates for Native Instruments Kontrol S MK3 keyboards** (`.kmt` files) from
instrument definitions, and lets you choose which knob or button each of the instrument's parameters is on.

It is built with wxWidgets using standard Windows controls, so it works with screen readers (NVDA, JAWS,
Narrator), and every command can be reached from the keyboard.

- [What it reads](#what-it-reads)
- [Building](#building)
- [Running](#running)
- [The window](#the-window)
- [Making a template](#making-a-template)
- [Menus and keyboard shortcuts](#menus-and-keyboard-shortcuts)
- [Undo and redo](#undo-and-redo)
- [The automatic layout](#the-automatic-layout)
- [Editing a control](#editing-a-control)
- [Templates the program writes](#templates-the-program-writes)
- [Opening an existing template](#opening-an-existing-template)
- [Instrument file formats](#instrument-file-formats)
- [Screen reader support](#screen-reader-support)
- [Using K-MIDI on a Mac](#using-k-midi-on-a-mac)
- [Limitations and things not yet known](#limitations-and-things-not-yet-known)
- [Source code](#source-code)
- [Tests](#tests)

## What it reads

| File | What K-MIDI uses from it |
| --- | --- |
| Cakewalk instrument definitions (`.ins`) | Controller (CC) names. RPN and NRPN names are listed but can't be placed (see [Limitations](#limitations-and-things-not-yet-known)). |
| Asp instruments (`.toml`) | Controller names, ranges and value names, and virtual instruments' macros. |
| REAPER bank files (`.reabank`) | Bank and preset names only: these files have no controller names. |
| Kontrol MIDI templates (`.kmt`) | A template made earlier, or made in Komplete Kontrol, to change and save again. |

All four formats also read presets (banks and programs), but presets aren't written into templates yet.

## Building

You need:

- Visual Studio 2022 with the C++ desktop workload (it includes CMake)
- [vcpkg](https://github.com/microsoft/vcpkg), with the `VCPKG_ROOT` environment variable set to where it is

vcpkg installs the libraries listed in `vcpkg.json` the first time you configure: wxWidgets, nlohmann-json (JSON)
and toml++ (TOML).

```
cmake --preset windows-vs2022
cmake --build --preset windows-release
ctest --preset windows-tests
```

This makes `build/windows-vs2022/Release/K-MIDI.exe`. To work in Visual Studio, open
`build/windows-vs2022/KMidi.sln`; KMidi is the startup project. Use `windows-debug` in place of
`windows-release` for a debug build.

On macOS (Xcode and vcpkg), use the `macos-xcode` presets, which make `K-MIDI.app`:

```
cmake --preset macos-xcode
cmake --build --preset macos-release
ctest --preset macos-tests
```

The macOS version hasn't been built or tried with VoiceOver yet: see [Using K-MIDI on a Mac](#using-k-midi-on-a-mac).

### After moving or renaming the project folder

This only affects a folder you've already built in. A fresh clone has no `build/` folder (it isn't in the
repository), so its first configure makes one for wherever it was cloned.

CMake writes the project folder's full path into everything it generates under `build/`, including the Visual
Studio solution and the Xcode project. If you move or rename the folder, those files still point at the old
place and the build fails. Configuring again doesn't fix this either, because CMake won't reuse a build folder
that was set up for a different source folder. Close the solution or project, delete the preset's build folder,
then configure again:

```
rmdir /s /q build\windows-vs2022
cmake --preset windows-vs2022
```

On macOS, run `rm -rf build/macos-xcode` and then `cmake --preset macos-xcode`. Nothing in `build/` needs to
be kept: it's all generated, and vcpkg reinstalls its libraries from its cache in a few seconds.

## Running

Start `K-MIDI.exe`, or give it files to open:

```
K-MIDI.exe "C:\Instruments\Nord Drum Poly.toml"
K-MIDI.exe "My Synth.kmt"
```

A `.kmt` file opens as a template, and any other file as an instrument. If you give it both, put the instrument
first (see [Opening an existing template](#opening-an-existing-template)).

## The window

From top to bottom, and in Tab order:

1. **Template name** (Alt+N): the name Komplete Kontrol shows for the template. It starts as the instrument's name.
2. **MIDI channel** (Alt+I), 1 to 16: the channel that every knob and button sends on. The keys, wheels, pedals and
   touch strip are also set to it.
3. **Colour** (Alt+O): the template's colour.
4. **Parameters** list (Alt+P): the instrument's parameters, in this order: CCs, then RPNs, then NRPNs, each in number
   order. Its columns are:
   - *Parameter*: the name.
   - *Sends*: the message and number, e.g. "CC 74".
   - *Range*: "0 to 127", or "5 values" for a parameter whose values have names.
   - *Placed on*: the control it is on, e.g. "Page 1, knob 3"; blank if it isn't placed; "Can't be sent" for RPNs
     and NRPNs.
5. **Place on a knob** (Alt+K) and **Place on a button** (Alt+U) buttons.
6. **Template controls** list (Alt+T): every control of every page. Each page lists knobs 1 to 8, then buttons 1 to 8, so
   a template with 2 pages has 32 rows. Its columns are:
   - *Control*: e.g. "Page 2, button 4".
   - *Label*: the text the keyboard shows for it; blank for an empty control.
   - *Sends*: e.g. "CC 74", followed by the parameter's name in brackets if the label is different; "Empty" for an
     empty control.
   - *Settings*: for a knob, its range and "percent" if it shows its value as a percentage, e.g. "0 to 100,
     percent". For a button, what it does and its colour, e.g. "Toggles 0 and 127, Mint" or "Steps 0 to 39 by 1
     and wraps, Mint".
7. **Move left** (Alt+V), **Move right** (Alt+R), **Switch knob/button** (Alt+S), **Edit...** (Alt+D) and
   **Clear** (Alt+A) buttons, which act on the control selected in the template controls list.
8. The **status bar**, which shows the last message spoken (see
   [Screen reader support](#screen-reader-support)).

The window's title is the template's name, with "(changed)" after it when there are unsaved changes.

## Making a template

1. **Open an instrument** with File > Open instrument (Ctrl+O).
   - An `.ins` file usually defines many instruments, so you are asked to choose one.
   - A virtual Asp instrument is filled in from the instrument it is based on (see
     [Virtual instruments](#virtual-instruments)).
   - The parameters are placed on the controls automatically (see [The automatic layout](#the-automatic-layout)).
     You hear how many were placed on how many pages, e.g. "Opened Nord Drum (DJJ): 14 controllers placed on 2
     pages." If the instrument has RPNs or NRPNs, the status bar says how many can't be placed.
   - Opening an instrument replaces the current template. If it has unsaved changes, you are asked whether to save
     them first.
2. **Rearrange the controls** in the template controls list:
   - **Alt+Left / Alt+Right** move the selected control one place left or right, swapping it with the control it
     moves over (which may be empty). Moving right from knob 8 goes to knob 1 of the next page, and moving left
     from knob 1 goes to knob 8 of the previous page. Knobs only swap with knobs, and buttons with buttons. Moving
     right from the last control of the last page adds a page. Moving left from the first control says "Already
     the first control."
   - **Alt+Page Up / Alt+Page Down** move the control to the same place on the previous or next page, swapping it
     with the control there. Moving down from the last page adds a page.
   - **Ctrl+B** switches the control between a knob and a button. It goes to the same place in the other row if
     that is empty, or else to the first empty control after it (adding a page if needed). It keeps its label,
     but its range and behaviour are reset for its new kind of control.
   - **Enter**, **Ctrl+E** or a double click edits the control (see [Editing a control](#editing-a-control)).
   - **Delete** or **Backspace** clears the control.
   - The selection follows a control you move, so your screen reader reads it in its new place.
   - An empty control can't be moved, switched or edited: you hear "This control is empty."
3. **Place parameters** from the parameters list. Select one or more parameters (Shift and Ctrl select several),
   then:
   - **Enter**, **Ctrl+P** or a double click places each on a knob, or on a button if it is a switch (see
     [Switches](#switches)).
   - **Ctrl+K** (or the Place on a knob button) places them on knobs.
   - **Ctrl+Shift+K** (or the Place on a button button) places them on buttons.

   Each parameter goes on the first empty control of that kind starting at the control selected in the template
   controls list. If that control is a different kind (a button when placing on a knob, say), it starts at the
   first control of that kind on the same page. If there are no empty controls left, a page is added. Several
   parameters are placed one after another, in the order they are listed. A parameter that is already placed
   moves to its new control, so each parameter is on one control at most. You hear where it went, e.g. "Cutoff is
   on page 1, knob 3." RPNs and NRPNs aren't placed, and a message lists them.
4. **Add or remove pages** from the Layout menu: Add page (Ctrl+Shift+N) adds an empty page at the end, and Remove
   empty pages at the end does what it says, always keeping at least one page. **Automatic layout** (Ctrl+L) lays
   out the whole template again, after asking; it keeps the template's name, channel and colour.
5. **Undo** a change with Edit > Undo (Ctrl+Z), and redo it with Edit > Redo (Ctrl+Y). See
   [Undo and redo](#undo-and-redo).
6. **Set the template's name, MIDI channel and colour** at the top of the window.
7. **Save the template** with File > Save template (Ctrl+S). The first time, you choose where. The file name
   starts as the template's name, with any of `\ / : * ? " < > |` changed to `-`, and `.kmt` is added if you don't
   type an extension. Save template as (Ctrl+Shift+S) saves to a new file. Copy or import the file into Komplete
   Kontrol as you would any other MIDI template.

When you close the window with unsaved changes, you are asked whether to save them.

## Menus and keyboard shortcuts

F1 (Help > Keyboard shortcuts) lists the most-used shortcuts.

**File**

| Command | Shortcut |
| --- | --- |
| Open instrument... | Ctrl+O |
| Open template... | Ctrl+Shift+O |
| Save template | Ctrl+S |
| Save template as... | Ctrl+Shift+S |
| Exit | |

**Edit**

| Command | Shortcut |
| --- | --- |
| Undo (the menu item names the change, e.g. "Undo Move Cutoff right") | Ctrl+Z |
| Redo | Ctrl+Y |

**Control**

| Command | Shortcut |
| --- | --- |
| Place parameter (on a knob, or a button for switches) | Ctrl+P, or Enter in the parameters list |
| Place parameter on a knob | Ctrl+K |
| Place parameter on a button | Ctrl+Shift+K |
| Edit control... | Ctrl+E, or Enter in the template controls list |
| Move left | Alt+Left |
| Move right | Alt+Right |
| Move to previous page | Alt+Page Up |
| Move to next page | Alt+Page Down |
| Switch between knob and button | Ctrl+B |
| Clear control | Delete or Backspace in the template controls list |

**Layout**

| Command | Shortcut |
| --- | --- |
| Automatic layout | Ctrl+L |
| Add page | Ctrl+Shift+N |
| Remove empty pages at the end | |

**Help**

| Command | Shortcut |
| --- | --- |
| Keyboard shortcuts | F1 |
| About K-MIDI | |

The Control menu's commands act on the selected parameters or the selected template control wherever the focus
is, so on Windows you can, for example, press Alt+Right while in the parameters list. (On a Mac, the move
shortcuts work only in the template controls list: see [Using K-MIDI on a Mac](#using-k-midi-on-a-mac).)

## Undo and redo

Every change to the template can be undone with Edit > Undo (Ctrl+Z) and redone with Edit > Redo (Ctrl+Y), as
many times as you like: placing, moving, switching, editing and clearing controls, adding and removing pages, the
automatic layout, and changing the template's name, MIDI channel or colour.

- The Undo and Redo menu items name the change they undo or redo, e.g. "Undo Move Cutoff right", and you hear the
  same when you use them.
- Undoing puts the selection in the template controls list back where it was before the change, without moving
  the focus. Redoing puts it where it was after the change.
- Typing in the template name field, or changing the MIDI channel a step at a time, counts as one change, so one
  undo takes back the whole name you typed. Ctrl+Z in the name field undoes through the template's history, not
  just the text in the field.
- Undoing back to the template as it was last saved (or opened) removes "(changed)" from the window's title, and
  you won't be asked to save it.
- Opening an instrument or a template starts a new history: you can't undo opening a file.
- With nothing to undo or redo, you hear "Nothing to undo." or "Nothing to redo."

## The automatic layout

The automatic layout is made when you open an instrument, or with Layout > Automatic layout.

**For a virtual instrument** (an Asp `.toml` with `[macros]`), only the macros are placed, each on the knob for
its encoder number: encoder 0 is page 1, knob 1; encoder 7 is page 1, knob 8; encoder 8 is page 2, knob 1; and so
on. Encoders without a macro are left empty, so gaps in the numbering stay as gaps. Macros that name an RPN or
NRPN are left out. This is how Asp puts a virtual instrument's macros on its encoders.

**For any other instrument**, every CC is placed in number order, switches on the buttons and everything else on
the knobs, filling page 1, then page 2, and so on. Left out are:

- RPNs and NRPNs, which templates can't send
- CC 0 and CC 32 (bank select)
- CC 6 and CC 38 (data entry) and CCs 98 to 101 (RPN and NRPN numbers)
- CCs whose names say they are unused, by the same rules as Asp: names starting with `***`, empty names, and
  names such as "Undefined", "Unused", "Not used", "Reserved", "Res", "None", "N/A", "-", "12 - Undefined" or
  "Something (unused)"

You can still place any of the left-out CCs yourself.

### Switches

A parameter is a switch, and goes on a button, if it has a range of 0 to 1, or its name contains (ignoring case)
"on/off", "off/on", "on-off", "switch", "sw", "toggle", "64-127=on" or "on=127". These are Asp's rules.

### Placed controls

A parameter placed on a control (by the automatic layout or by you) starts with:

- its name as the label
- its range, clamped to 0 to 127; 0 to 127 if the file gives none
- for a knob: its value shown as a whole number
- for a button: toggle between the lowest and highest values if it is a switch, has 2 values or fewer, or has
  more than 32 values; otherwise step through its values one at a time, going back to the lowest after the
  highest. Buttons are Mint.

## Editing a control

Edit a control with Enter, Ctrl+E or a double click in the template controls list. At the top, the dialog says
what the control sends, e.g. "Sends CC 81 (LFO Shape). Values: 0 triangle, 1 saw", including the parameter's value
names if it has any.

For a **knob**:

- **Label**: the text the keyboard shows.
- **Lowest value** and **Highest value** (0 to 127): the values it sends at each end of its travel.
- **Show value as**: *Whole number* or *Percent*.

For a **button**:

- **Label**.
- **When pressed**: *Toggle between the lowest and highest values* (each press switches between them), or *Step
  through the values* (each press sends the next value).
- **Lowest (off) value** and **Highest (on) value** (0 to 127): for a toggle, the values it sends when off and
  on; for stepping, the first and last values.
- **Step** (1 to 127): how far each press moves, when stepping.
- **Go back to the lowest value after the highest**: when stepping, whether it starts again after the last value.
- **Colour**: the button's colour.

Step and Go back to the lowest value are only available when stepping. The lowest value can't be above the
highest: if it is, OK says so and leaves the dialog open.

## Templates the program writes

A `.kmt` file is JSON text. K-MIDI writes it as Komplete Kontrol does: UTF-8, keys in alphabetical
order, 4-space indents, Windows (CRLF) line endings, and no line ending at the end of the file.

Each page has 8 knobs and 8 buttons:

- A **knob** sends its CC on the template's channel with an absolute range (its lowest and highest values), and
  shows its value as a whole number or a percentage.
- A **button** sends its CC, either toggling between two values or stepping through a range, and has a colour.
- A placed control's label is marked as a custom label.

**Empty controls** can't simply be left out of a page, so each is given a CC that the instrument doesn't use,
labelled "CC n" and not marked as a custom label, which is how Komplete Kontrol shows an unassigned control. That
way, touching an empty knob can't change one of the instrument's parameters. These CCs are taken in this order
from the CCs that MIDI leaves undefined: 14 to 31, 102 to 119, 85 to 90, 3 and 9, skipping any the instrument
names and any placed in the template. In the unlikely case that none are left, they are used again anyway.

The rest of a **new template** is set as Komplete Kontrol sets it:

- Keys: the whole keyboard, no transposition, the template's colour
- Mod wheel: CC 1, 0 to 127
- Pitch bend wheel: pitch bend, 0 to 16383
- Touch strip: CC 5, 0 to 127
- Expression pedal: CC 11, 0 to 127
- Sustain pedal: CC 64 on the tip and CC 65 on the ring
- The two assignable pedal inputs: unassigned
- The template name is shown, and the template has no picture

All of these are sent on the template's MIDI channel.

## Opening an existing template

File > Open template (Ctrl+Shift+O) opens a `.kmt` file, made by K-MIDI or by Komplete Kontrol, so you can
rearrange it and save it again.

- Its name, colour, MIDI channel (from the keys' channel) and pages are read in. Up to 8 knobs and 8 buttons are
  read from each page.
- Controls with Komplete Kontrol's own "CC n" labels (not custom labels) are read as empty, as are controls that
  send anything other than a CC.
- Its settings for the keys, wheels, pedals and touch strip, its picture, and anything else in the file are kept
  as they are when you save it. Only the name, colour, pages and MIDI channels are changed.

To place more of an instrument's parameters on a template, **open the instrument first, then the template**:
opening an instrument replaces the template with an automatic layout, but opening a template keeps the instrument
that is open. With the instrument open, the template controls list shows the parameter names of the CCs the
template sends, and the parameters list shows where each is placed.

## Instrument file formats

### Cakewalk instrument definitions (.ins)

K-MIDI reads `.ins` files as Asp does:

- Sections start with a line such as `.Controller Names`, and lists within them with `[List Name]`. Lines that
  start with `;` are comments.
- The instrument chosen from `.Instrument Definitions` names its lists with `Control=`, `RPN=` and `NRPN=`. A list
  can be `BasedOn=` another, whose entries it adds to or replaces.
- CC numbers above 127 are ignored.
- Presets come from `Patch[bank]=` (and `Patch[*]=` for any bank), with `BankSelMethod=` saying how banks are
  selected, including Cakewalk's built-in `0..127` and `1..128` lists. An instrument with no `Patch` entries uses
  the patch names list with its own name, as General MIDI does.
- A file that isn't valid UTF-8 is read as Windows-1252, as older `.ins` files are.

When you open an `.ins` file, only that file is read, so a list it takes from another `.ins` file isn't found.
When looking for the instrument a virtual instrument is based on, all the `.ins` files searched are read together,
so their lists can come from each other.

### Asp instruments (.toml)

From an Asp `.toml` file, K-MIDI reads:

- `[instrument]`: `name`, `based_on` (see below) and `bank_select`
- `[controllers]`, `[rpns]` and `[nrpns]`: each entry is either a name (`74 = "Cutoff"`) or a table with a `name`
  and optionally a `range = [minimum, maximum]` and `values`, which is a table of value names or the name of a
  table in `[value_lists]`. `default` is ignored. CC numbers outside 0 to 127 are ignored.
- `[value_lists]`
- `[presets]` and `[[banks]]` (with `number`, `name` and `presets`)

Other tables, such as `[notes]` and `[cv]`, are ignored. A file with a syntax error, a parameter without a name, a
range whose minimum is above its maximum, or `values` naming a missing value list can't be opened, and the message
says which line or entry is wrong.

#### Virtual instruments

A virtual instrument has `based_on` in `[instrument]`, naming the instrument it copies, and a `[macros]` table
naming the parameters to put on the encoders (`0 = "Cutoff"`). When you open one, K-MIDI finds the
instrument it is based on by name (ignoring case and surrounding spaces) by searching, including subfolders:

1. if the file is in an Asp checkout (a folder above it has `data/instruments` or `config/instruments`), the
   checkout's `config/instruments` and `data/instruments` folders
2. the file's own folder

`.toml` instruments are searched before `.ins` instruments, as Asp prefers them. A virtual instrument can be based
on another virtual instrument. If the instrument isn't found, you are asked to choose the file that defines it.

Macros are matched to the base instrument's parameters by name, ignoring case and surrounding spaces. A message
lists any macros that name parameters the instrument doesn't have; those are left out.

### REAPER bank files (.reabank)

```
// A comment
5 Preset before any bank
Bank 0 1 Factory B
0 Init Patch
1 Big Pad
Bank * * Any bank
127 Last
```

`Bank <MSB> <LSB> <name>` starts a bank, and each following `<program> <name>` line is a preset in it. `*` (or
anything that isn't a number from 0 to 127) means no bank select value. Lines starting with `//` are comments. The
instrument is named after the file.

Because these files name no controllers, opening one tells you how many presets it names and that there is
nothing to put on the knobs.

## Screen reader support

- All the controls are native controls (standard Windows controls, or AppKit controls on a Mac) with visible
  labels. The two lists are named "Parameters" and "Template controls". Each field is also given its label as
  its accessible name, as VoiceOver doesn't name a field from the label beside it.
- After moving, switching, editing or clearing a control, the selection and focus follow it, so your screen
  reader reads it in its new place.
- Messages that don't move the focus, such as where a parameter was placed or "Already the first control", are
  spoken and shown in the status bar. On Windows they are UI Automation notifications, which NVDA, Narrator and
  recent versions of JAWS speak; on a Mac they are VoiceOver announcements.
- Each list row holds everything about one control in its columns, e.g. "Page 1, knob 3", "Cutoff", "CC 74",
  "0 to 127", so a screen reader set to read list columns reads the whole control.
- The access keys on the window (Alt+N, Alt+I, Alt+O, Alt+P, Alt+K, Alt+U, Alt+T, Alt+V, Alt+R, Alt+S, Alt+D,
  Alt+A) don't clash with the menus (Alt+F, Alt+E, Alt+C, Alt+L, Alt+H).
- Errors and questions use standard message boxes.

## Using K-MIDI on a Mac

On a Mac, Ctrl in the shortcuts is Command and Alt is Option (Help > Keyboard shortcuts shows them that way), and
redo is Command+Shift+Z.

These are the changes made for VoiceOver:

- **The lists are native tables.** On Windows the lists are Windows list views. wxWidgets draws its list view
  itself on macOS, which VoiceOver can't read, so on a Mac the lists are wxDataViewCtrl tables, which are native
  AppKit tables that VoiceOver reads row by row.
- **Every field has an accessible name.** VoiceOver doesn't name a field from the label beside it, so the template
  name, MIDI channel and colour fields, and every field in the Edit control dialog, are named explicitly. Number
  fields are made of a text field and a stepper, and both parts are named.
- **Messages are VoiceOver announcements**, so you hear where a parameter was placed, what was undone, and so on.
- **The move keys are handled by the template controls list itself**, not by the menu. Option+Left/Right and
  Option+Page Up/Down move the selected control when the template controls list has the focus. They aren't menu
  shortcuts on a Mac, because Option+Left/Right move by word in a text field and a menu shortcut would take them
  from the template name field. The commands are still in the Control menu, without shortcuts.
- **Enter and Delete are handled by the lists too**, as Mac tables don't send the "activated" event that Windows
  list views send for Enter. Enter places a parameter or edits a control; Delete clears a control.

Other things to know on a Mac:

- Turn on keyboard navigation (System Settings > Keyboard > Keyboard navigation) so that Tab moves to the lists
  and buttons as well as the text fields.
- F1 may need the Fn key. The shortcuts are also listed under Help.
- Exit and About K-MIDI are in the K-MIDI application menu, where macOS puts them.

**What has been checked:** the Mac list code (wxDataViewCtrl) builds and works on Windows, using the CMake option
`KMIDI_TABLE_USE_DATAVIEW=ON`: moving controls, undoing and redoing, and saving templates. The VoiceOver code
(`Announce_mac.mm`) follows code already used with VoiceOver in another wxWidgets program.

**What hasn't been checked:** K-MIDI hasn't been built on a Mac or used with VoiceOver. When it is, check that
VoiceOver reads the lists' rows, the fields' names and the announcements, and that Option+Left/Right move a
control in the template controls list.

## Limitations and things not yet known

- **Kontrol S MK3 only.** The Kontrol S MK2 stores MIDI templates differently (all in one settings file) and
  isn't supported.
- **Only CCs.** Kontrol S MK3 templates can't send RPNs or NRPNs (Native Instruments staff have said NRPN isn't
  supported), so those parameters are listed but can't be placed.
- **Program changes aren't written yet.** Kontrol S MK3 buttons can send program changes, but no sample template
  with one has been found to show how a `.kmt` file stores it. So presets read from any file aren't put on
  buttons. A `.kmt` saved by Komplete Kontrol with a program change button would make it possible to add this.
- **Colours.** Violet, Fuchsia, Red and Mint have been seen in templates made by Komplete Kontrol. Orange,
  Yellow, Lime, Green, Cyan, Turquoise, Blue, Plum, Purple, Magenta and White follow Native Instruments' colour
  names but haven't been tested. If Komplete Kontrol rejects a template, try one of the confirmed colours.
- **Picture.** New templates have no picture. A template opened from a `.kmt` file keeps its picture.
- **Label length.** The keyboard's displays may shorten long labels. K-MIDI doesn't limit their length,
  so edit any that are too long.
- Switching a control between knob and button resets its range and behaviour.
- Opening an instrument replaces the open template with an automatic layout.
- The Mac version hasn't been built or tried with VoiceOver yet (see
  [Using K-MIDI on a Mac](#using-k-midi-on-a-mac)).

## Source code

```
src/core/            the template logic, with no user interface
  Instrument.*       instruments, parameters and presets; Asp's switch and unused-name rules; reading text files
  InsLoader.*        Cakewalk .ins files
  TomlLoader.*       Asp .toml files
  Reabank.*          REAPER .reabank files
  Library.*          opening any instrument file; finding the instrument a virtual instrument is based on
  Layout.*           pages of knobs and buttons; placing, moving and switching controls; the automatic layout
  Kmt.*              reading and writing .kmt files
  History.*          undo and redo
src/app/             the wxWidgets program
  App.cpp            start-up and command line
  MainFrame.*        the main window, menus and commands
  SlotDialog.*       the Edit control dialog
  TableView.*        the lists: a Windows list view, or a native table (wxDataViewCtrl) on macOS
  Announce.h         screen reader helpers: spoken messages and accessible names
  Announce_msw.cpp   Windows: spoken messages (UI Automation notifications)
  Accessibility_msw.cpp   Windows: accessible names (MSAA dynamic annotation)
  Announce_mac.mm    macOS: VoiceOver announcements and accessible names
  Announce_generic.cpp    other platforms: does nothing
tests/               tests and their instrument files
samples/kmt/         a template made by Komplete Kontrol, which the .kmt format was worked out from
```

The sample template, for a Korg opsix, is from
[laserbeak/Komplete-Kontrol-MK3-Stuff](https://github.com/laserbeak/Komplete-Kontrol-MK3-Stuff). The `.ins` test
files are copied from Asp's tests.

## Tests

`ctest --preset windows-tests` runs `KMidiTests`, which checks:

- reading `.ins` files: lists from other files, `BasedOn`, lists based on each other, bank select methods, and
  built-in patch lists
- reading `.toml` files: ranges, value names and value lists, presets, and virtual instruments found in a library
  folder, including an unknown macro
- reading `.reabank` files
- the automatic layout, and moving, switching and placing controls
- undo and redo: restoring the layout and selection, joining typed changes into one, and knowing when the
  template is back to how it was saved
- `.kmt` files: the sample reads correctly and writes back with the same structure and formatting; a new template
  has every key the sample has; empty controls get CCs the instrument doesn't use; a template reads back as it was
  written

To check a whole folder of instrument files, such as an Asp checkout's instruments:

```
KMidiTests --library "C:\path\to\Asp\data\instruments"
```

This reads every `.ins`, `.toml` and `.reabank` file in the folder and its subfolders, resolves virtual
instruments, makes the automatic layout and writes a template for each instrument, then reports any file that
failed and any unknown macros.
