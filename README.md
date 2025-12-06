# LocateYourPhoto

![Concept Explanation](assets/concept.png)

Did you ever try to browse your photos in a map? Have you ever been annoyed by the instability of Location Information Linkage functionality in your S**y camera? No worries! You can use your phone, your smartwatch, or anything else that records your track to add GPS coordinates to your photos. A GPX trace file is all you need, and this application will do the rest.

Personally, I use [Geo Tracker](https://geo-tracker.org/en/) (Android/iOS) to record my tracks on my phone. GPX file from other apps should also work.

## Features

- ✅ **Interactive GUI** with map visualization showing GPX track and photo locations
- ✅ Parse GPX trace files and extract GPS trackpoints
- ✅ Match photo timestamps with GPS coordinates using intelligent interpolation
- ✅ Support for both JPG and RAW files (ARW, NEF, CR2, DNG, HEIC, AVIF, CR3, JXL, and more)
- ✅ Adaptive maximum time difference based on GPX precision
- ✅ Dry-run mode to preview changes without modifying files
- ✅ Optional overwrite of existing GPS data
- ✅ Drag & drop support for photos
- ✅ Visual workflow guide in the interface
- ✅ Real-time progress tracking and status updates
- ✅ Detailed format compatibility warnings

## Requirements

### Build Dependencies

- **CMake** 3.20 or higher
- **Qt6** with the following components:
  - Core
  - Widgets
  - Quick
  - QuickWidgets
  - Positioning
  - Location
- **exiv2** - For EXIF metadata reading and writing
- **pugixml** - For GPX file parsing
- **C++17** compatible compiler

### Optional Dependencies

- **ExifTool** - Required for some modern formats (HEIC, AVIF, CR3, JXL). If not available, these formats will be skipped with a warning.

### Installing Dependencies

#### Unix-like systems

Varies by your distribution. Don't forget the system dependencies!

#### Windows
- Install Qt6 from [qt.io](https://www.qt.io/download)
- Install exiv2 and pugixml using vcpkg or download pre-built binaries
- Add Qt6 and dependencies to your PATH

## Building

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
cmake --build .
```

The executable will be created as `LocateYourPhoto` (or `LocateYourPhoto.exe` on Windows) in the build directory.

## Usage

### Workflow

The application provides a guided workflow in the left panel:

1. **Load GPX File**
   - Click "Load GPX File" button or use `Ctrl+G`
   - Select your GPX trace file
   - The map will display the GPS track and show the number of trackpoints loaded

2. **Add Photos**
   - Click "Add Photos" button or use `Ctrl+O`
   - Select one or more photo files
   - Or drag and drop photos directly into the application window
   - Photos will appear in the list with their current status

3. **Adjust Settings** (if needed)
   - **Time Offset**: Adjust if your camera clock doesn't match the GPX timezone
   - **Dry Run**: Enable to preview changes without modifying files
   - **Overwrite GPS**: Enable to replace existing GPS coordinates
   - **Advanced Settings**: Access via Settings menu or button for:
     - Maximum time difference for GPS matching (0 = automatic)
     - Force interpolation (ignore time threshold)

4. **Process Photos**
   - Click "Process" button to start adding GPS coordinates
   - Progress is shown in the status bar
   - Photos are marked on the map as they are processed
   - A summary dialog appears when processing completes

### Keyboard Shortcuts

- `Ctrl+G` - Load GPX file
- `Ctrl+O` - Add photos
- `Ctrl+Q` - Quit application

### Map Interaction

- The map automatically centers on the GPX track when loaded
- Click on photos in the list to highlight their location on the map
- Photo markers show which photos have been successfully processed
- The GPX track is displayed as a blue line

### Settings Explained

#### Time Offset
Apply a timezone offset (in hours) to align photo timestamps with GPX timestamps. For example:
- Camera clock in UTC+8 (Asia/Shanghai) while GPX is in UTC → use `8` hours
- Camera clock in UTC-5 (EST) while GPX is in UTC → use `-5` hours

#### Maximum Time Difference
Maximum time difference (in seconds) between photo timestamp and GPS trackpoint for matching. Set to `0` for automatic calculation based on GPX trackpoint interval (3× average interval, between 60-600 seconds).

#### Force Interpolate
Always interpolate between trackpoints regardless of time difference. This ignores the maximum time difference threshold.

#### Dry Run
Preview what would happen without actually modifying any files. Useful for testing settings before processing.

#### Overwrite GPS
Replace existing GPS coordinates in photos. By default, photos with existing GPS data are skipped.

## How It Works

### 1. GPX Parsing

The application parses your GPX trace file and extracts all trackpoints with their timestamps and coordinates (latitude, longitude, elevation).

### 2. Adaptive Time Matching

- Calculates the average interval between GPS trackpoints
- Sets maximum time difference to 3× average interval (between 60-600 seconds)
- Can be overridden in Advanced Settings

### 3. GPS Matching Algorithm

For each photo:

1. Extracts the capture timestamp from EXIF data
2. Finds the GPS trackpoints immediately before and after the photo time
3. Uses linear interpolation to calculate precise coordinates
4. Falls back to nearest trackpoint if photo is at the edge of the trace

### 4. EXIF Writing

- Writes GPS coordinates in standard EXIF format
- Uses **exiv2** for most formats (JPEG, TIFF, DNG, PNG, and common RAW formats)
- Uses **ExifTool** for tricky formats (HEIC, AVIF, CR3, JXL) when available
- Preserves all existing EXIF data
- Converts decimal degrees to degrees/minutes/seconds format

## Supported File Formats

### Full Support (exiv2)
- JPEG (.jpg, .jpeg)
- TIFF (.tif, .tiff)
- PNG (.png)
- DNG (.dng)
- Common RAW formats: ARW (Sony), NEF (Nikon), CR2 (Canon), ORF (Olympus), RW2 (Panasonic), and more

### Requires ExifTool
- HEIC (.heic, .heif)
- AVIF (.avif)
- CR3 (.cr3) - Canon RAW
- JXL (.jxl) - JPEG XL

### Limited Support
Some proprietary RAW formats may work but are marked as risky due to potential file integrity concerns.

### No Metadata Support
- BMP (.bmp)
- GIF (.gif)
- TGA (.tga)

The application will warn you about format compatibility before processing.

## Output

The application provides visual feedback:

- **Status Bar**: Shows current operation and progress
- **Photo List**: Displays each photo with its processing status
- **Map View**: Shows GPX track and photo locations
- **Summary Dialog**: Appears after processing with statistics

Example status messages:
- "GPX loaded: 1234 trackpoints"
- "144 photos added"
- "Processing photo 45 of 144..."
- "Complete: 120/144 photos updated"

## Edge Cases Handled

1. **Photos outside GPS trace time range**: Matched to nearest trackpoint if within max time difference
2. **Photos with existing GPS data**: Skipped by default unless "Overwrite GPS" is enabled
3. **Photos without timestamps**: Skipped with warning
4. **No matching GPS trackpoints**: Skipped with warning
5. **Timezone considerations**: All times treated as UTC; use Time Offset to align camera time with GPX time
6. **Format compatibility**: Warnings shown for formats that may not support GPS writing

## Limitations

- Photo timestamps must be in EXIF data
- GPS trace must cover (approximately) the same time period as photos
- RAW file writing may not work for all camera models (tested with Sony ARW, Nikon NEF, Canon CR2)
- Interpolation assumes linear movement between trackpoints
- ExifTool must be in PATH for HEIC/AVIF/CR3/JXL support

## Troubleshooting

### ExifTool Not Found
If you see warnings about ExifTool not being available:
- Install ExifTool from [exiftool.org](https://exiftool.org/)
- Ensure it's in your system PATH
- Formats requiring ExifTool will be skipped

### Map Not Displaying
- Ensure Qt6 Location module is properly installed
- Check that your system has internet access (for map tiles)
- Verify Qt6 Positioning and Location components are available

### Build Errors
- Ensure all Qt6 components are installed (especially Quick, QuickWidgets, Positioning, Location)
- Check that exiv2 and pugixml development packages are installed
- Verify CMake version is 3.20 or higher
- On Linux, you may need to set `CMAKE_PREFIX_PATH` to your Qt6 installation
