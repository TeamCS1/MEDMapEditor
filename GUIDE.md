# MEd map editor: user guide

This is the revived MEd (MapEditor v0.32) for **GTA III 1.0**. It lets you look at the whole map in 3D, select objects, move them, hide or remove them, and save the result without touching your game. It is safe by default: your game folder is never written to unless you explicitly ask for it.

Everything below was checked against the real GTA III files and the game's own code. Where something is a guess or untested in the game, the guide says so.

---

## 1. Key terminology (read this first)

| Term | What it means |
|---|---|
| **Game folder** | Your GTA III install (here `GTA_3_1.0`). The editor reads it and never changes it by default. |
| **`gta3.dat`** | The game's master list (`data/gta3.dat`). It names every IDE, IPL and COL file the game loads, one per line. If a file is not listed there, the game ignores it. |
| **IMG archive** | `models/gta3.img` (with its index `gta3.dir`): one big file holding the 3D models and textures. Entries are 2048-byte sectors. |
| **DFF** | A 3D model (for example `hospitaland.dff`). Lives inside the IMG. |
| **TXD** | A texture dictionary: the pictures a model uses. Also inside the IMG. If a model's TXD is missing the game falls back to `generic`. |
| **COL** | Collision: the invisible shape the game uses for walking and driving into things (`COLFILE` lines in `gta3.dat`). Every object in the original game has one. |
| **IDE** | *Item definition* file. For every object it says: **model id**, DFF name, TXD name and **draw distance** (plus flags). Think of it as the catalogue of object types. |
| **IPL** | *Item placement* file. It says **where** objects are: one line per object in an `inst` section. This is the file the editor changes. |
| **Placement** (or **instance**) | One line in an IPL: "put object #961 at x, y, z with this rotation". The map is 8,689 placements. |
| **Model id** | The number that links a placement (IPL) to its definition (IDE). The game uses the **id**; the model name written in the IPL is ignored. |
| **Region** | This guide's word for **one IPL file**: a chunk of the map (see section 6). The game itself has no such thing; it just loads every IPL listed in `gta3.dat`. |
| **Coordinates** | x and y are the position on the map, **z is the height**. North is +y, east is +x. |
| **Rotation (quaternion)** | Four numbers x, y, z, w. GTA III applies the *inverse* of the stored rotation. `w` must stay between -1 and 1, or the game builds a broken matrix. |
| **Draw distance** | How far from the camera the game still draws an object. Normal objects: under 300 units. LOD models: 600 to 3000. |
| **LOD** | *Level of detail*: a cheap, low-detail stand-in for a building, shown when you are far away. A building and its LOD are a **pair**: same name after the first three letters (`hospitaland` / `LODpitaland`), at the same spot. |
| **Placement slots (pools)** | The game has fixed room for placements: 5,500 buildings + 1,214 treadables + 2,802 dummies = **9,516 in total**. The original map uses 8,689, so about 800 are spare. The **Pools** button in the toolbar shows the use of each pool: **Buildings** (ordinary map pieces), **Treadables** (pieces with path data for peds and cars) and **Dummies** (objects listed in `object.dat`). The model decides the pool, not the place it is put, so a copy goes into the same pool as the original. Each pool is full on its own: if any one overflows the game crashes, even when the others have room. In the original map Buildings use 5,140 of 5,500, Dummies 2,336 of 2,802, and **Treadables 1,213 of 1,214, so only one more path-carrying model fits.** |
| **Below the map** | Moving an object to height **-200** instead of deleting it. It is the safe way to "delete" (see section 5). |
| **Undo step** | Everything you do to one selection counts as one step. The editor keeps the last **20**. |
| **Check Map** | The wizard that looks for things the game would skip, reject or crash on (section 7). |
| **`MEd_saved`** | The folder next to your game folder where saved files go by default. |

---

## 2. The window

**Open another folder:** **File > Open...**. It asks for a GTA III folder, then starts a new editor window on it and closes this one (it warns first if there are unsaved changes).

**Starting the editor:** double-click `med_viewer.exe`. It reopens the game folder you used last (remembered in `med_viewer.ini` next to the program) or asks you to choose one. To open a different folder, give it on the command line, for example `med_viewer.exe "C:\Games\GTA III"`; that folder becomes the remembered one. To load your mod map for further editing, open the game folder the mod is applied to (for example `GTA3 Project RAGE\ProjectUpstate`).

```
+------------------------------------------------------------------------------+
| File v   Save All   Reference v                                              |  top row
| <- Undo  Redo ->  | New Object  Centre  Duplicate  Delete  | LOD view         |  editor tools
+-----------------+--------------------------------------+---------------------+
| SCENE panel     |                                      | INSPECTOR panel     |
| (regions, LOD)  |            3D view                   | (the selected       |
|                 |                                      |  object's details)  |
+-----------------+--------------------------------------+---------------------+
```

- **Top row (files and checks):** **File** drops down Open..., Save All (Ctrl+S), Save as Mod..., View a DFF / TXD / COL file..., Import Model... and **Image Archive...** (see below); **Save All** is also a button of its own; **Reference** drops down Pools and Check Map (F7); **View** drops down **Show hidden regions**, **Show traffic paths** and **Full Screen** (F11) (see below); **Bookmarks** opens the bookmarks list. Save All is greyed out until something has changed.
- **Editor toolbar (second row):** Undo (back arrow), Redo (forward arrow), New Object..., Centre on Object, Duplicate, Delete and the LOD view toggle. Buttons are greyed out until they can do something (Undo only after an edit, Delete, Duplicate and Centre only with a selection).
- **Left column, three tabs:** **Scene** (which regions are drawn, plus the LOD controls), **Placements** (find, list and pick placed objects) and **Item** (every object definition; see below).
- **3D view (middle):** the map.
- **Properties panel (right):** what the selected object is (texture, file, LOD partner), its **Model ID** (with **Choose...**), **Position** and **Rotation** boxes with `<<` `<` `>` `>>` buttons, **Rotate -90 / +90**, **Reset**, **Apply typed values**, **Centre on Object** and the two deletes. See "Properties panel" below.
- The **window title** starts with the name of the project (the folder you opened, for example `ProjectUpstate`; File > Open... starts a new window on the folder you pick, so the title follows), then shows the camera position, the object count, the frame rate and what is selected.

It starts at the first of MEd's own bookmarks (Portland Hospital).

---

## 3. Moving around

| Do this | To |
|---|---|
| **W / S** or **Up / Down** | Move forward / back |
| **A / D** or **Left / Right** | Move left / right |
| **Ctrl + W/S**, **Page Up / Page Down**, **R / F** | Move up / down |
| **Shift** (while moving) | Move faster |
| **Left mouse button, drag** | Look around |
| **Ctrl + left mouse button, drag** | Slide the camera |
| **Mouse wheel** | Forward / back (Shift = faster) |
| **Space** | Centre the camera on the selected object |

---

## 4. Selecting and editing

- **Double-click** an object to select it. A red box, coloured axes and a dashed circle appear around it, and the Inspector fills in.
- Small things (traffic cones, bins) can be double-clicked a few pixels beside them: the editor looks up to about 6 pixels around the cursor for a small object that is in front of the ground or wall behind it.
- **Shift + double-click** adds or removes objects from the selection.
- **Double-click empty ground**, or press **Esc**, to clear the selection.
- **Right-click** an object for its menu (Centre on Object, **Properties...** (its item definition: draw distance, flags, times; see "Editing an item definition"), DFF Properties, View TXD, Collision Properties, Extract Object..., Duplicate Object, Replace Object..., Copy, Delete options). With several objects selected (or Shift + right-click) the menu offers Duplicate, Replace, Copy, Extract and Delete for all of them.
- **Move it:**
  - **Alt + left mouse button, drag** slides the selected objects over the ground.
  - **Alt + arrow keys** (or W/A/S/D) nudge them; **Alt + Ctrl + arrows** or **Alt + Page Up/Down** move them up and down.
  - In the **Inspector** type exact X, Y, Z and press **Apply position**, or use the **X/Y/Z +10 / -10** buttons.
- **New object:** the toolbar **New Object...** button (placed at the middle of the view) or right-click on empty ground > **New Object...** (placed where you clicked). First click the region it should go into in the Scene list (or select an object, and its file is used); the region must be ticked. A window lists every object from the IDE files with its ID, texture dictionary and draw distance; type to search by name, texture dictionary or ID, use the arrow keys, press Enter or double-click, or press Place. The new object stands on the surface under the click with no rotation, is selected, and the view centres on it. One undo step. It takes a slot in the game's pool for its model (see Pools); Check Map warns when a pool is over its original size.
- **Copy and Paste:** select objects, then **Copy** (toolbar, **Ctrl+C**, or right-click an object > Copy). Then **Paste** (toolbar or **Ctrl+V**, which pastes at the middle of the view) or right-click empty ground > **Paste Object** (pastes where you clicked). As with New Object, first click the region the copies should go into in the Scene list (or select an object, and its file is used); the region must be ticked. Like MEd, the **last** object you copied lands on that point and the others keep their positions relative to it, with their own rotation. **Only the position across the map moves: the height of each copy stays what the original had**, so on higher or lower ground, adjust the height afterwards (Z +10 / -10 or the Z box). A building and its LOD stand-in are copied together. The pasted objects are selected so you can move them straight away. One undo step. The copy is remembered until you copy again, even if you delete the originals.
- **Properties panel (right side):**
  - **Position X / Y / Z** and **Rotation X / Y / Z** (angles in degrees; Z is the heading, the turn seen from above; X and Y tilt the object). Type a value and press **Enter** (or **Apply typed values**); **Esc** puts the stored value back. A box you are typing in is not overwritten.
  - The **`<<` `<` `>` `>>`** buttons next to each box step it, like MEd: position by 10 / 0.1 units, rotation by 10 / 0.2 degrees. **Hold a button to repeat.** A run of steps on the same selection is one undo step.
  - **Rotate -90 / Rotate +90**: a quarter turn (clockwise / counter-clockwise seen from above). **Reset**: no rotation at all.
  - **Model ID** with **Choose...**: give the selected objects another model (the same as **Replace Object**, see below); an unknown ID is refused.
  - With **several objects selected**, the boxes show the first one; the buttons move and turn every one of them (each about its own position); a typed position moves the group so the first lands on it; a typed angle is given to all of them.
  - A building and its **LOD stand-in move and turn together**, everywhere (panel, Alt+drag, Alt+keys).
  - Bad input (a word, an empty box, `5,5`) shows a short warning and changes nothing.
- **Rotate:** the Properties panel (above), or Rotate -90 / +90 there.
- **Duplicate:** the toolbar **Duplicate** button, **Ctrl+D**, or right-click > Duplicate Object. Like MEd, the copy sits **exactly on top of the original** (same model, position and rotation), so **move it right away**: the copies (including the LOD stand-in of a building, which MEd does not copy) are selected, so Alt + drag or the Inspector moves the pair together. Copies you forget to move are reported by Check Map as duplicate placements. Every copy takes a slot in the game's pool for its model (see Pools); a copy of a model with path data uses a scarce *treadable* slot.
- **LOD view** (toolbar toggle **LOD view**, or **Ctrl+L**): with an object selected, shows its LOD stand-in in place of the normal model, at any distance, so you can see what the far version looks like. If the selection has no LOD version the title says so and nothing changes. Switch it off to return to normal.
- **Undo / Redo:** the toolbar buttons, or **Ctrl+Z** and **Ctrl+Y**. The last 20 steps are kept.

---

### The Placements tab (finding objects)

The **Placements** tab (left column, next to Scene; **Ctrl+F** opens it with the cursor in the search box) is like a layers panel: it lists the objects of a region, and lets you find any object by name or ID.

- **Region** (top): pick a region to list its objects. This is also **where New Object and Paste put things**, like the file you chose in MEd's IPL tab.
- **Search**: type part of a name (`cone`) or a model ID (`1360`); the list filters while you type. **This region / All regions** decides where it looks (All regions adds a Region column).
- **Click** a row to select that object in 3D (**Ctrl** or **Shift** for several). **Double-click** or **Enter** flies the camera to it.
- **< and >** (or **Shift+F3 / F3**, or **Enter** in the search box) step to the previous / next match, select it and fly there. **Esc** clears the search.
- **Click a column header** to sort (again for the reverse order).
- It works the other way too: when you select something in the 3D view, the list jumps to its region and highlights it.
- Picking an object of a **hidden** region (unticked in the Scene tab) ticks that region on, so you can see what you picked.
- If you sort by X, Y or Z and then move or delete objects, the list re-sorts when the edit is finished (after letting go of an Alt+drag), so rows do not jump under the mouse while you drag.
- **New... / Duplicate / Delete** under the list do the same as the toolbar (Delete is the safe one: below the map).
- Hover over any control for a short explanation.

---

### View > Show hidden regions

Two files that `gta3.dat` lists as regions hold no objects at all: `GTA3.ZON` (zones) and `CULL.IPL` (cull boxes). They are **hidden** from the Scene list and the Placements region picker unless **View > Show hidden regions** is ticked. Click it to tick it, click again to untick; it is **off** every time the editor starts. Only regions that were empty when the map was loaded are hidden: a region you make with **New region...** is always listed, even while it is still empty.

### Bookmarks (saved places on the map)

**Bookmarks** on the top row (or **Ctrl+B**) opens MEd's bookmarks list. The window can stay open while you fly around; every change is saved at once.

- **Add...**: keeps the current camera position. It suggests a name in MEd's style (`GTA III - <region> - `); type the rest and press Enter.
- **Rename...**, **Remove** (asks first; the **Delete** key too), **Move Up / Move Down** (the order of the list).
- **Move to location** (or **double-click**, or **Enter**): flies the camera there.
- The editor starts at the **first** bookmark, so move your favourite place to the top.
- They are kept in `bookmarks.ini` next to the editor, in MEd's own format (you can copy bookmarks between MEd and this editor). The first time, it starts with MEd's three bookmarks; the Vice City and San Andreas ones point at empty sea in GTA III, so you can remove them.

### The Item tab (every object you can place)

The **Item** tab (MEd's name for it) lists every object definition from the IDE files, placed or not.

- **Search** by part of a name (`tree`), a texture dictionary (`dyncones`) or a model ID (`1360`); the list filters while you type. **Enter** jumps into the list, **Esc** clears.
- **Show**: all items, only the ones **placed on the map**, or only the ones **not placed yet**.
- Columns: **Name**, **ID**, **Placed** (how many times it is on the map), **Pool** (which game pool its placements use: Building, Treadable or Dummy; Treadable is nearly full on the original map, so watch those), **Texture**, **Draw** distance and the **IDE file**. Click a header to sort.
- **Add to map**: puts the picked item in the region chosen in the Placements tab, in the middle of the view (like New Object, without the pop-up list).
- **Show placements** (or double-click / Enter): opens the Placements tab with every placement of that item, in all regions.
- **Extract...**: copies the item's files into a folder (see below).
- **Edit Definition (draw distance, flags, times)...**: the Item Definition Entry window (see below).
- Selecting an object in the 3D view highlights its definition here.

### Full screen (View > Full Screen, or F11)

The 3D view fills the whole screen, without toolbars or panels. As in MEd it asks first; answer **Yes**. While it is on you can only look around (the mouse and the movement keys work); nothing can be selected, moved or deleted. To get back: **double-click**, or press **Enter**, **Esc** or **F11**. The window comes back exactly as it was (also when it was maximised).

### Editing an item definition (draw distance, flags, times)

The **Item Definition Entry** window (MEd's name) changes a model's line in its IDE file. Open it with right-click on an object > **Properties...**, or in the Item tab with **Edit Definition...**.

- **ID Number**, **Model Name** and **Object Count** are shown but cannot be changed here: the placements, `gta3.img` and the collision all refer to them.
- **Texture Name**: the texture dictionary. It must exist in `gta3.img` (otherwise the change is refused). The model is redrawn with it at once.
- **Draw distance (LOD)**: how far away the game still draws it (one box per part of the model; most have one). Decimals are kept (Rockstar uses some, such as 152.25). 0 to 10,000. Above 300 the game may treat a model as a "big building", the way it treats LOD models (the window says so).
- **Time On / Time Off**: for objects that only appear at certain hours (street lights, lit windows). Both at 00:00 means an ordinary object; anything else makes it a time-controlled one (`tobj`). On and off cannot be the same hour (the game would never show it).
- **Parameters**: the flags as one number, with a box per flag; ticking a box changes the number and the other way round. The boxes say what each flag does **in GTA III**: 1 normal cull, 2 no fade-in, 4 draw last (for transparency), 8 additive blending, 16 subway object, 32 ignore lighting, 64 no depth writes. MEd also offers 128 to 1024 (Vice City flags); GTA III ignores them, so the boxes say "not used by GTA III".
- Every change applies at once (as in MEd). A box that holds something invalid shows **Not applied: ...** under the boxes and nothing changes until it is right.
- **Undo** goes back to what the window opened with. **Save** (or Enter) keeps the change and closes; **Cancel** (or Esc, or the X) undoes and closes.
- The IDE file is written by **Save All** (with the other changed files, the original kept once as `<file>.med-backup`) or **Save as Mod...**. Until then the title shows `*` and the editor's title says UNSAVED. Putting a value back to what it was makes the file unchanged again.
- **Add to Map**, **DFF Properties**, **View TXD** and **COL Properties** do what their names say for this model.
- IDE edits are not part of the map's Undo / Redo (the window has its own Undo).

### The Image Archive (gta3.img: delete, rename, add, extract, rebuild)

**File > Image Archive...** opens MEd's IMG tool on the game's `models/gta3.img` (the archive that holds every model `.dff` and texture dictionary `.txd`). The other archives of the game folder are listed on the left; **Open...** adds any other `.img`.

- The list shows each file's **Name**, **Offset**, **Size**, **Filetype** (3D Model, Texture Dictionary, Collision File, Binary IPL File) and what you have changed. **Show** filters by type, **Find** by part of the name; click a header to sort.
- **Delete File(s)** (or Del): asks first, and names the item definitions that use the file and how often they are placed. Without its model the game cannot draw those objects (Check Map will report them).
- **Rename File...** (or F2): at most 23 characters, no spaces. Definitions still look for the old name, and it says so.
- **Import File(s)...**: adds files from disk. A file whose name is already in the archive asks: **Yes** replace it, **No** skip that file, **Cancel** stop.
- **Extract File(s)...**: copies the selected files into a folder (byte for byte).
- **Open File** (or Enter, or double-click): shows a model, texture dictionary or collision file in the viewers.
- **Nothing is written until you press Rebuild Archive.** The status line says "Please Rebuild Archive!" and the title shows `*` while there are changes. Switching archives or closing asks: **Yes** rebuild now, **No** forget the changes, **Cancel** stay.
- **Rebuild Archive** writes a new archive next to the old one, checks that every file reads back exactly, and only then puts it in place. The archive as it was is kept as `gta3.img.med-backup` and `gta3.dir.med-backup` (the first time; later rebuilds keep that first backup). If anything goes wrong (for example the game has the archive open) the old archive stays exactly as it was. Close the game and the Mod Manager before rebuilding. The editor uses the new archive at once.
- **Removing an imported model:** delete its `.dff` (and its `.txd` if nothing else uses it) here and rebuild. Then remove its definition line and collision as `models/med-imports.txt` describes. After a rebuild that file gets a note that its "cut gta3.img back" steps no longer apply: do not follow those any more.

### Replace Object (swap a placed object for another model)

Right-click an object > **Replace Object...** (or **Replace Objects...** with several selected), or press **Choose...** next to Model ID in the Properties panel. A list of every object opens with the current model highlighted; type to search, then press **Replace** (or double-click). The object keeps its position and rotation and stays selected. Typing a number in the Model ID box and pressing Apply does the same.

A building and its **LOD stand-in** (the low-detail copy the game draws from far away) are replaced as a pair:

| The old building... | The new model... | What happens to the LOD stand-in |
|---|---|---|
| has a stand-in | has a LOD model | it becomes the new model's LOD model, in the same place |
| has a stand-in | has no LOD model | it goes, so the old building no longer shows from far away: a stand-in from the **original game** is moved below the map (like Delete, the safe way); one **you added** (by Duplicate, Paste, ...) is removed from the file, since the game never had it |
| has no stand-in | has a LOD model | one is added at the building's position (it takes a slot in the Building pool) |
| has no stand-in | has no LOD model | nothing |

**Original or added?** The editor knows which placements belong to the original GTA III map and which you added (Duplicate, Paste, New Object, Add to map, a stand-in added by Replace), also after saving and reopening. The Properties panel says which ("Part of the original GTA III map" or "Added by you"). It works this out from the files: it carries a fingerprint of every original placement (file, model and position, as a hash). An original you moved still counts as original. Anything doubtful counts as original, so it is only ever moved below the map, never removed. For a game other than GTA III 1.0 everything counts as original.

When a copy sits exactly on top of its original (right after Duplicate or Paste), each building is paired with its own stand-in: an original with the original one, a copy with the copied one. So moving, deleting or replacing the copy never takes the original's stand-in along.

The LOD model of a building is the one named `LOD` plus the building's name without its first three letters (`hospitaland` -> `LODpitaland`, `SupaSave` -> `LODaSave`). The whole replace is one undo step. Picking the model the object already has changes nothing.

### Building roads (attach road pieces end to end)

GTA III's roads are built from a kit of pieces (models named `rd_...`): straights 5 to 50 long in three widths (20, 30 and 40), slopes, pieces that shift a lane sideways, corners, T-junctions and crossroads. The editor can join them for you so they line up exactly.

1. **Select a road piece** (double-click it). A ball appears on each of its ends: **green** = open, **grey** = already joined to another piece. A T-junction has 3 balls, crossroads 4.
2. **Click a green ball** (or press **Ctrl+R** for the first open end). A strip opens along the top of the 3D view with every piece that fits that end's width, each with a picture and a short description: "straight, 50 long", "climbs 9.85 over 50", "descends 5.00 over 50", "shifts left 25", "turns left", "T-junction: on, branch right", "crossroads". Pieces that can join in more than one way are shown once per way. The pictures are drawn with the joining end at the bottom, so you can see which way a corner turns.
3. **Hover a picture**: the piece is shown attached at the yellow ball, without placing it.
4. **Click the picture** to place it. It goes into the same region as the piece you attached it to (with its LOD stand-in if the model has one), it is selected, and the strip moves on to its far end: keep clicking to build a road. Each piece is one undo step. **Esc** closes the strip; the mouse wheel scrolls it.

How it lines up: the editor measures where each piece's ends are, which way they face and how high the road surface is there from the model itself, so slopes chain correctly (a `Road2C50` climbs 9.85, not 10) and bends work too (`turn4` bends 22.5 degrees: four of them make a quarter circle; `RoadS1` is a diagonal S-bend; `Road2B50s` a skewed piece). This was checked against Rockstar's own map: 928 ends of the offered pieces meet their neighbours there to within 0.02 units, none is slightly off.

Which pieces are offered: 55 of the 64 kit pieces (all straights, slopes, lane shifts, bends, corners, T-junctions and crossroads). Not offered, because Rockstar's map shows their measured ends are wrong or never joins them to another kit piece: `CrossRda1rw22`, `CrossRda1w22`, `RoadInd1way40`, `opera_custma`, `opera_custmb`, `specialrd11`, `specialrdmall`, `warehsearea`, `CrossRoadsa24` (one-offs built for one spot). `Road1C50kb` has only one measurable end. A tilted piece cannot take a new piece. Built roads are ordinary placements: Check Map, Delete, Undo and Save treat them like everything else.

Tip from the user's first road: when you remove a tunnel or bridge, check the ground there. Terrain is part of the map too, and a road built where a tunnel was runs inside the hill.

### Traffic and pedestrian paths (View > Show traffic paths, and the path budget)

Cars and pedestrians in GTA III follow paths that come with the road pieces themselves: each piece of the `rd_` road kit (and many other road and pavement models) carries a few path nodes for cars and for pedestrians in its IDE data. When the game starts it joins the placed pieces into one network: two pieces join where their connection points are less than 1 unit apart. Place road pieces end to end (the road builder does that exactly) and the traffic carries on from one to the next.

**View > Show traffic paths** draws that network in the 3D view, exactly as the game builds it:

| Colour | Meaning |
|---|---|
| Orange lines | car paths (thick where two pieces join) |
| Light blue lines | pedestrian paths |
| Red marks | a car path ends here with nothing to join: traffic cannot continue |
| Pink marks | the same for pedestrians |
| Grey dots | pedestrian nodes the game drops because nothing joins them |

Use it to check a new road: no red mark at a joint means cars can drive on from the old road onto the new one.

**The path budget.** The game keeps the whole network in small fixed tables, and Rockstar's map fills them almost completely:

| Table | Size | The original map uses |
|---|---|---|
| Path nodes (car + pedestrian) | 4930 | 4923 |
| Car path links | 2076 | 2071 |
| Connections | 10260 | 10220 |
| Road objects (placed pieces with paths) | 1250 | 1213 |

So a handful of extra road pieces already overflows them, and the game then crashes while loading, typically with "Unhandled exception c0000005 at 00429caa". No limit adjuster raises these tables for GTA III. The editor shows them in three places, worked out exactly the way the game does it:

- **Check Map**: an error for every table the map overflows, and notes for how full each one is.
- **The Pools window**: a "Path tables" section with bars.
- **The road builder strip**: how many nodes and car links are left, and what the piece under the mouse adds.

A plugin that raises these tables changes the limits. If its settings file (`III.PathLimits.ini`) is in the game folder next to `III.PathLimits.asi`, the editor uses its sizes; otherwise the original ones. The editor does not come with such a plugin.

### Looking at a model, its textures and its collision (the three viewers)

As in MEd, each object definition has three viewers. Open them from the **Item** tab (pick an item, then **DFF Properties**, **View TXD** or **View Collision**) or by right-clicking an object on the map (**DFF Properties...**, **View TXD...**, **Collision Properties...**). Each opens in its own window, several can be open at once, and none of them changes anything.

- **DFF Properties** (the 3D model): the model on its own. **Right mouse button** drag turns it, **left mouse button** drag moves it, the **wheel** zooms (Shift: faster); **Zoom In**, **Zoom Out**, **Reset View**, **Automatically Rotate**, **Show Wireframe**, **Show Object Number** (for models made of several parts). The table lists objects, vertices, faces, colours, UV mapping, normals, materials, textured materials, triangle splits and their format, night vertex colours and 2DFX (both Vice City / San Andreas features, "No" for GTA III), and every texture the model uses.
- **TXD Properties** (the textures): every texture of the dictionary with its size; click one to see it. It is drawn over a grey checkerboard so transparent parts show (**Show alpha**), fitted to the window or at its real size (**Fit to window**). Below: texture name, alpha name, size, pixel format (for example "8-bit palette of 888"), mipmaps, alpha, platform, and which object definitions use the dictionary.
- **Collision Object Properties** (what the game treats as solid): the collision boxes, spheres and triangle mesh, each coloured by its surface type with MEd's colours (grey for concrete and default, green grass and hedges, blue water and glass, sandy sand, brown wood), inside the red bounds box with the green / red / blue axes and the bounding circle. **Show Boxes**, **Show Spheres**, **Show Collision Mesh**, **Wireframe**; the table gives the counts, the bounds, the surface types used and the COL file it came from. (Show Shadow Mesh is a San Andreas feature.)

**Files from disk** (to check a model before it goes into the game): **File > View a DFF / TXD / COL file...** (several at once are fine), or drag .dff, .txd and .col files from Explorer onto the editor window. A model finds its textures in a .txd with the same name next to it, or in the only .txd in that folder; the table says which (or that none was found). A .col file with several collision models gets a **Model in file** list to pick one. Opening the same file again reads it again, so you can check a change made in another program. Nothing is copied into the game.

Differences from MEd, on purpose: the view turns about the middle of the model and starts at a distance that fits it (MEd: about the model's origin, always 52 units away, so a cone was a dot), and the picture keeps its true shape (MEd stretched it to a square).

### Importing your own models

**File > Import Model (DFF + TXD + COL)...** and pick the model's `.dff`. The Import Model window opens with the `.txd` and `.col` of the same name beside it already filled in (or the only `.txd` in that folder). It checks everything again as you type and lists what still stops the import under **Not ready yet**, or says **Ready to import** with a summary of what will be written. The **Import** button only works when nothing is wrong.

| Part of the window | What it does |
|---|---|
| **Model / Textures / Collision** | the three files; **Browse...** picks another, **View** opens it in the viewers first, **No collision file** clears the collision |
| **Without a collision file it starts as** | only when there is no `.col`: **Empty** (a clean slate: nothing is solid, you would walk and drive through it until you build its collision), **One box around the model** (solid straight away; good for simple props such as a bin), **Copy the model's triangles** (solid in exactly the model's shape; good for roads and buildings) |
| **Name** | the model's name in the game: up to 19 letters, digits, `_` or `-`, and not used by any model yet (starts as the file name) |
| **Model ID** | must be free and below 5500, the size of the game's model table; **Suggest** gives the first free one (GTA III uses 0 to 3152, so new models start at 3153) |
| **Draw distance, Flags** | the IDE values (200 and 0 to start with) |
| **Region** | **your own regions only** (made with **New region...**, also here); Rockstar's files are not changed |

The rules it checks:

- **Textures are required.** A dictionary with the same name as one already in the game is reused only if it is the very same file (then it is shared, not added again); a different file with that name is refused, so rename yours. The game has room for 850 dictionaries and uses about 720. A texture the model asks for that is not in the dictionary is a note (the game draws that part plain).
- **A collision file** may hold several models: the one named like the model is used (or its only model), and it is renamed to the model's name.
- **Where it goes.** The model and textures are added to the end of `models/gta3.img` (nothing already in it moves; `gta3.dir` is backed up first as `gta3.dir.before-<name>.med-backup`). The definition line goes into the region's own `data/maps/<region>/<region>.ide` and the collision into its own `<region>.col`; both are added to `data/gta3.dat` (backed up once as `gta3.dat.med-backup`). The collision file is listed with level 0, which the game keeps loaded everywhere. Every import is written down in `models/med-imports.txt` with how to undo it.
- **A game folder that shares its `gta3.img` with another folder** (a hard link, as test copies sometimes do) is refused, because the import would change the other folder too.

Afterwards a message lists the files that changed, and the model is in the **Item** tab straight away, ready to place with **Add to map** like any other (it takes a Building slot). Tested on a copy of GTA III: imported models load on the next start, are drawn with their textures, and Check Map reports no problem with them. Not tested yet in the game itself.

### Editing collision (the Edit tab)

Collision is what the game treats as solid: you walk on it, cars hit it. Open a model's collision (Item tab **View Collision**, right-click an object > **Collision Properties...**, or a `.col` file from disk) and click the **Edit** tab. The model itself is shown as a faint grey outline, so you can build the collision around it (**Show the model's outline**).

**Selecting** (in the 3D view):

| Do this | Selects |
|---|---|
| Click | the box, sphere or mesh face under the mouse (a box remembers which side you clicked) |
| Shift + click | adds it, or removes it again |
| Double-click a mesh face | its whole flat side (all the joined triangles in the same plane) |
| Ctrl + drag | everything inside the rectangle |
| Click empty space, or Esc | nothing |

Right-drag still turns the view and a left-drag on empty space still moves it, as on the View tab.

**Changing it:**

- **Push / pull** (as in SketchUp): drag a selected face, box side or sphere towards you or away. It moves in steps of 0.01 (hold Shift for 0.1). Or type a distance and press **Pull out** / **Push in**.
  - A **box side** moves on its own, so the box gets longer or shorter. A box is never made thinner than 0.05.
  - **Mesh faces** move along their direction. Sides that run along that direction stretch with them (pulling the top of a box-shaped mesh makes it taller). Elsewhere, new side walls join the moved faces to the rest, so the mesh stays closed.
  - A **sphere** gets bigger or smaller.
- **Add Box / Add Sphere**: a new box or sphere around the model, already selected. For a bin or a dumpster, one box is usually all it needs: Add Box, then pull its sides to fit.
- **Move** shifts the selection by the X, Y, Z you type. **Apply typed size** sets the exact Min / Max of one selected box, or the centre and radius of one sphere.
- **Delete selected** (or Del) removes it. Deleting mesh faces can leave holes.
- **Surface material**: what the surface is made of. The game uses it for sounds, bullet-hit effects and how well tyres grip. Each entry shows the number, its name (for example 2 Grass, 19 Water, 22 Solid wood), its grip group, MEd's colour for it, and how many shapes of this model use it. With nothing selected it sets the material for the next new box or sphere.
  - The grip groups are the rows of the game's `data/surface.dat`: road, hard, loose, wet and rubber. Grass, gravel, sand, hedges and cardboard are "loose", water alone is "wet", and the wood and pavement types grip like road.
  - The names are the modding community's (from re3). Which group each number falls in comes from the game's own code, and it agrees with every name. Numbers 33 and 34 are not used in GTA III.
  - Tip: open a similar Rockstar object in the collision viewer and use the same material as it does.
- **Open edges** are drawn in red: edges that only one face uses, so the mesh has a hole or a border there. A building's walls should normally be closed; ground pieces have borders by nature.
- **Undo / Redo** (Ctrl+Z / Ctrl+Y), up to 50 steps.

**Saving** (Save, or Ctrl+S): Rockstar's own COL files are never changed.

- **One of Rockstar's models:** a copy goes into a COL file of one of **your** regions, picked under **Save into region**. It is `data/maps/<region>/<region>_lvl1.col` for Portland (level 1), `_lvl2` for Staunton Island, `_lvl3` for Shoreside Vale, and `<region>.col` for models that are loaded everywhere (level 0). `data/gta3.dat` gets a `COLFILE` line for it after Rockstar's (backed up first).
  - Why this works: the game loads every collision file in `gta3.dat` order, and a later record of the same model replaces the earlier one. When you cross to another island it reloads that island's level, so your copy must have the same level to stay in force. This was checked in the game's own code; it hasn't been tried in the game yet.
  - Every placement of that model in the whole map gets the new collision.
- **A model you imported, or one you saved before:** saved straight into your own file.
- **A `.col` file from disk:** saved back into that file. The other models in it are kept byte for byte, and the original is kept once as `.med-backup`.

Closing the window with unsaved changes asks whether to save them.

### Extract Object (copy an object's files out of the game)

Right-click an object > **Extract Object...** (or **Extract Objects...** with several selected), or pick an item in the Item tab and press **Extract...**. Choose a folder; the editor remembers it for next time.

Each object gets its own subfolder named after the model, holding **copies**:

| File | What it is |
|---|---|
| `<model>.dff` | the 3D model, byte for byte as stored in `models/gta3.img` |
| `<txd>.txd` | its texture dictionary (from `gta3.img`, or `models/generic.txd`) |
| `<model>.col` | its collision, copied out of the area's COL file as a valid one-model COL file |
| `<model>.txt` | its IDE line, IDE file, pool, how often it is placed and where each file came from |

Nothing is removed or changed in the game: the archive and the COL files are only read (the tests check that their size and date stay the same). If files from an earlier extract are already in the folder you are asked before they are replaced. Afterwards a summary lists what was copied and anything that could not be found (for example a model without collision), and offers to open the folder. Several selected placements of the same model give one copy.

The original MEd copied only the `.dff` and `.txd`. Uses: editing a model in a 3D tool, making a new object from an existing one, or keeping a reference copy. The DFF and TXD can end in some zero bytes, because the archive stores files in 2048-byte blocks; MEd's copies were the same and tools read them normally.

---

## 5. Deleting objects (read this before you delete anything)

MEd's own readme warns that deleting objects "still causes random crashes in game". The editor therefore offers two kinds of delete:

1. **Delete (move below the map), the default.** The object **and its LOD partner** are moved to height -200 and left in the file. Tested by hand in the game: **-200 is safe, -2000 crashed it.** Use this one. Keyboard: **Del**.
2. **Delete permanently.** The object **and its LOD partner** are removed from the IPL file. Keyboard: **Shift + Del** (it asks first). *This has not been tested in the game.* Test a copy of your map before you trust it.

Why the LOD matters: a building and its LOD are a pair. Moving or removing only one of them leaves an orphan, which is the most likely cause of the game's crashes. Both delete options handle the pair for you, and Check Map warns if a pair gets split.

---

## 6. Regions, the Scene panel and LOD models

### What a region is

GTA III's map is stored in several IPL files, each holding the placements for one part of the city. The Scene panel lists them with a **checkbox each**. Only checked regions are drawn and can be picked. That is how MEd saves frame rate on big maps.

| IPL file | Placements | What is in it |
|---|---|---|
| `INDUSTNE`, `INDUSTNW`, `INDUSTSE`, `INDUSTSW` | 543 / 723 / 726 / 678 | Portland (four quarters) |
| `COMNtop`, `COMNbtm`, `COMSE`, `COMSW` | 1,038 / 903 / 566 / 781 | Staunton Island (four quarters) |
| `LANDne`, `LANDsw` | 956 / 1,225 | Shoreside Vale (two halves) |
| `overview` | 94 | Things that must be visible from anywhere: the five island-sized stand-in meshes, subway tunnel pieces, buoys, rock patches |
| `props` | 456 | Small clutter spread around: cardboard boxes, scaffolding, barriers, cones, bins, wrecks |
| `CULL`, `GTA3.ZON` | 0 | No placements: zone and cull data only |

### The two special files: `overview` and `props`

These two are **not tied to a place**. They are grouped by *kind*, and that is useful to know when you are looking for something:

- **`overview`** (94 placements, 23 different models) holds objects that have to be visible **from anywhere** on the map:
  - the five **island-sized stand-in meshes** (`IslandLODInd`, `IslandLODcomIND`, `IslandLODcomSUB`, `IslandLODsubCOM`, `IslandLODsubIND`): huge low-detail copies of whole islands, seen from across the water;
  - **subway tunnel pieces** (`sbwy_tunl_bit` 23 times, `sbwy_tunl_bend` 20 times);
  - **buoys** (19) and **rock patches** (`rockpatch03`, `rockpatch1`);
  - two lift-bridge LOD pieces.
  Only 2 of its 23 models are also placed in the normal region files, so `overview` really is its own set.
- **`props`** (456 placements, 61 different models) holds **small clutter** scattered around, mostly in and around Portland:
  - the biggest groups are `iten_details7` (70), **cardboard boxes** (62), **barriers** (39), **wrecked cars** (`lofucked_car` 26, `car_fucko` 18), **traffic cones** (17), bins (13) and dumpsters (11);
  - also scaffolding, billboards, barrels and a bomb-shop door.
  **28 of those 61 models are also placed in ordinary region files**, which shows the split is a convention and not a rule: the same kind of object can sit in either place.

Practical consequences: `overview` and `props` are listed in the Scene panel like any other region, so if you untick the district regions one by one, those objects stay on screen until you untick those two files as well (**Deselect all** clears all twelve); and if you cannot find a box or barrier in the region you expect, look in `props`.

The file names hint at the area, but the rectangles overlap (some files also hold LOD stand-ins spread around), so "region" means "one IPL file", not a strict map square.

### Using the Scene panel

- **Checkbox:** draw this region or not.
- **Select all / Deselect all:** all regions at once.
- **Only the selected row:** show just the highlighted region.
- **Double-click a region:** fly the camera to it.
- Objects in a hidden region cannot be selected, and hiding a region clears a selection inside it.

### LOD models (far stand-ins)

- **Show LOD models** turns the far stand-ins on or off.
- The **slider** sets the closest distance (0 to 1,500 units) at which a stand-in may appear. Default 350. A stand-in that has a building partner also waits until the building has dropped out of its own draw distance.
- If you ever see a low-detail grey mesh drawn **on top of** the real terrain next to you, raise the slider.
- What counts as a stand-in: a model named `LOD...`, an `IslandLOD...` mesh, or anything the IDE gives a draw distance of 500 or more.

---

## 7. Check Map (the toolbar button, or F7)

Press it **before you start the game**. It checks the map as it **would be saved**, including edits you have not saved yet. Problems are listed with the worst first; **Go to** (or double-click a row) jumps to the object. **Copy report** puts the text on the clipboard.

What it looks for, and what the game does about it:

| Problem | What the game does |
|---|---|
| A placement line has fewer than 12 values | Silently skips the line: the object does not exist |
| A model id that no IDE defines | Silently ignores the placement |
| Rotation `w` outside -1..1 | Builds a broken rotation matrix |
| Height far below the map (under -250) | Crashed the game at -2000 |
| Model missing from `gta3.img` | The game cannot load it |
| Texture dictionary missing | Falls back to `generic` (wrong textures) |
| No collision model for an object | Possible crash (not verified) |
| A listed file is missing | The game cannot open it |
| `INST` or `END` in capitals | The game only understands lowercase, so the section is ignored |
| A line longer than 255 characters | The game cuts it off |
| More than 9,516 placements | Not enough slots |
| A LOD without its building, or a building without its LOD, **that the original game did not have** | Possible crash (the likely cause of "delete" crashes) |
| An exact duplicate placement that the original game did not have | Wasted slot |

Your unmodified game gives **no errors and no warnings**. The original has 221 LODs without a building, 42 buildings without a LOD and 6 duplicates, and those are not reported.

---

## 8. Saving

- **Ctrl+S** or the **Save All** button writes only the files that changed: IPL files (placements) and IDE files (item definitions changed in the Item Definition Entry window).
- By default they go to a **separate folder** (`MEd_saved`, next to the game folder), in the same folder layout as the game. Your game folder is **not touched**.
- Only the placement lines are rewritten; everything else in the file (zone, cull, comments) is kept exactly as it was. An unchanged file saves **byte for byte identical** to the original.
- To use the result in the game, copy the saved files over the game's files (keep the original first).
- `--in-place` (a command-line option) overwrites the game's own files instead, after copying each original to `<file>.med-backup`.
- If a file cannot be written (for example it is read-only) you get an error. Nothing is lost silently.

### Mods

Switching mods on and off, launching the game with chosen mods, and the limit adjuster are done by the separate **GTA3 Mod Manager** (its own folder and guide). From the editor, **Save as Mod...** (toolbar) saves your changed map files (IPL and IDE) into a folder you pick, for example your mod's folder under the Mod Manager's `mods` folder. To keep building on earlier edits, open the editor on the game folder the mod is applied to.

---

## 9. Rules of thumb for safe editing

1. **Keep a building and its LOD together.** Move them together, delete them together (the editor does this for you).
2. **Use "move below the map"** instead of permanent delete.
3. **Stay under 9,516 placements.**
4. **Run Check Map** before every game test.
5. **Test in the game** after each batch of edits, and keep the original game folder as your backup.
6. **New models only through Import Model.** It writes the model, textures and collision the way the game reads them, with backups and an undo note. Rockstar's own IDE files change only when you edit a definition yourself (one line, backed up), and their COL files never.

---

## 10. Questions

### Why are some objects in `overview` and `props` and not in a region?
The game has no concept of regions. At start-up it simply loads **every** IPL listed in `gta3.dat`, one after another, and treats all placements the same. Rockstar split the four districts by **area**, and then put two files together by **kind**: `overview` holds things that have to be seen from anywhere, and `props` holds scattered clutter. So those two are not tied to a place, only to a purpose. (Of the 61 prop models, 28 are also placed in ordinary region files, so the split is a convention, not a rule.)

### What happens if I put a prop in a region instead of `props` or `overview`?
Nothing, as far as the game is concerned: it loads the object the same way wherever the line is. The only differences are for you:
- In this editor, hiding that region hides the prop too (handy while editing).
- Keep a building and its LOD in the **same file** when you can. In the original data 755 of 759 pairs are in the same file. The game is not known to require it, but it keeps things tidy.

### How many regions can I create? Is there a limit?
I found **no fixed limit on the number of IPL files** in the game's loader: it reads each `IPL` line from `gta3.dat` and loads that file, with no table of files. The real limit is the **9,516 placement slots** shared by all files. The original map uses 8,689. One cosmetic side effect: the loading bar is hard-coded for 73 steps (one per `gta3.dat` line plus fixed steps), so extra lines make the bar run past 100%.
This is from reading the game's code, not from testing a modified game.

### How do I create a new region?
In the **Scene panel** (left), press **New region...**, type a name (letters, digits, `_` and `-`) and OK. The editor makes an empty IPL file `data/maps/<name>/<name>.ipl` in the folder you opened and adds the line `IPL DATA\MAPS\<name>\<name>.IPL` to `data/gta3.dat` (the original `gta3.dat` is kept once as `gta3.dat.med-backup`). The new region appears in the list straight away, selected, with a **check box like every other region**: untick it to hide it and its objects, tick it to show them again.

To put objects into it: double-click the objects in the 3D view, click the new region in the list, press **Move selected objects here** (a building and its LOD stand-in move together), then **Save**. Both files (the one the objects came from and the new one) are rewritten. One undo step.

**Save as Mod...** also takes the new regions along (the `gta3.dat` that lists them and their IPL files) so the whole thing travels as one mod. Because the loading bar is fixed at 73 steps, each extra IPL line makes it run one step past the end at the very last step; that is only cosmetic.

By hand, if you ever need to: create the file with the two lines `inst` and `end`, add the `IPL` line to `gta3.dat`, then put placements (12 values per line) between them.

### The game crashes at 99% of the loading bar. What does that mean?
The bar counts loading steps out of 73. 99% (98.6%) is step 72, "Start script". The game has by then read every IPL, IDE and COL file and linked the LODs, so the crash is in what happens next: running the first frame of the main script and updating the camera. It is probably not a bad line in an IPL. **Caution:** if your edit added files to `gta3.dat`, the total is no longer 73, so "99%" may be a different step. What exactly crashes there is not yet known.

### The program says "entry point not found" or will not start.
That was a build problem (fixed): the program is now self-contained. If you still see it, tell me the exact message.

---

## 11. What the editor cannot do yet

removing an imported model in one step (for now: Image Archive for its files, then `models/med-imports.txt` for its definition line and collision), changing a model's ID or name, Vice City and San Andreas. Not tried in the game yet: an edited IDE definition and a rebuilt `gta3.img`. See `CLAUDE.md` for the full status and for how each part was recovered from the original program.
