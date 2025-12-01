# LocateYourPhoto

![Concept Explanation](assets/concept.png)

Did you ever try to browse your photos in a map? Have you ever been annoyed by the instability of Location Information Linkage functionality in your S**y camera? No worries! You can use your phone, your smartwatch, or anything else that records your track to add GPS coordinates to your photos. A GPX trace file is all you need, and this script will do the rest.

## Features

- ✅ Parse GPX trace files and extract GPS trackpoints
- ✅ Match photo timestamps with GPS coordinates using intelligent interpolation
- ✅ Support for both JPG and RAW files (ARW, NEF, CR2, DNG)
- ✅ Adaptive maximum time difference based on GPX precision
- ✅ Dry-run mode to preview changes
- ✅ Optional overwrite of existing GPS data
- ✅ Detailed logging and statistics

## Installation

This project uses [uv](https://github.com/astral-sh/uv) for dependency management, and relies on [ExifTool](https://exiftool.org/) for robust RAW (ARW/NEF/CR2/DNG) metadata support. You should add it into the PATH first.

```powershell
# Sync Python dependencies
uv sync
```

## Usage

### Basic Usage

```powershell
# Process photos with default settings
uv run python main.py trace.gpx photos/

# Preview changes without modifying files (dry-run)
uv run python main.py trace.gpx photos/ --dry-run

# Overwrite existing GPS data
uv run python main.py trace.gpx photos/ --overwrite-gps

# Use custom maximum time difference (in seconds)
uv run python main.py trace.gpx photos/ --max-time-diff 120

# Apply camera timezone offset (in hours) so EXIF local time is converted to UTC
# Useful when the trace file does not contain timezone information
# Example: camera clock in UTC+8 (Asia/Shanghai) while GPX is in UTC -> use 8 hours
uv run python main.py trace.gpx photos/ --time-offset-hours 8

# Increase parallelism when processing many photos (0 = auto)
uv run python main.py trace.gpx photos/ --workers 8

# Force interpolation even when photos are far from nearest trackpoints
uv run python main.py trace.gpx photos/ --force-interpolate

# Search photos recursively
uv run python main.py trace.gpx photos/ --recursive
```

## How It Works

### 1. GPX Parsing

The script parses your GPX trace file and extracts all trackpoints with their timestamps and coordinates (latitude, longitude, elevation).

### 2. Adaptive Time Matching

- Calculates the average interval between GPS trackpoints
- Sets maximum time difference to 3× average interval (between 60-600 seconds)
- Can be overridden with `--max-time-diff` parameter

### 3. GPS Matching Algorithm

For each photo:

1. Extracts the capture timestamp from EXIF data
2. Finds the GPS trackpoints immediately before and after the photo time
3. Uses linear interpolation to calculate precise coordinates
4. Falls back to nearest trackpoint if photo is at the edge of the trace

### 4. EXIF Writing

- Writes GPS coordinates in standard EXIF format
- Supports both JPG (using piexif) and RAW files (using exif library)
- Preserves all existing EXIF data
- Converts decimal degrees to degrees/minutes/seconds format

## Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `gpx_file` | Path to GPX trace file | Required |
| `photos_dir` | Directory containing photos | Required |
| `--overwrite-gps` | Overwrite existing GPS coordinates | Skip photos with GPS |
| `--dry-run` | Preview changes without modifying files | Off |
| `--max-time-diff SECONDS` | Maximum time difference for matching | Adaptive (3× avg interval) |
| `--time-offset-hours HOURS` | Time offset applied to photo timestamps to align with GPX (e.g. -8 when camera is UTC+8 and GPX is UTC) | 0 |
| `--workers N` | Number of concurrent worker threads for processing photos (0 = auto, 1 = no parallelism) | 0 |
| `--force-interpolate` | Always interpolate between trackpoints / use endpoints regardless of time gap (ignores max time diff) | Off |
| `--recursive` | Search subdirectories | Off |

## Output

The script provides detailed logging:

- Number of trackpoints parsed from GPX
- Time range of GPS trace
- Processing status for each photo
- Summary statistics (updated, skipped, etc.)

Example output:

```log
2025-12-01 22:15:00 - INFO - Parsing GPX file: trace.gpx
2025-12-01 22:15:00 - INFO - Parsed 1234 trackpoints
2025-12-01 22:15:00 - INFO - Time range: 2025-12-01 07:35:10 to 2025-12-01 09:37:53
2025-12-01 22:15:00 - INFO - Average trackpoint interval: 5.8 seconds
2025-12-01 22:15:00 - INFO - Using adaptive max time difference: 17 seconds
2025-12-01 22:15:00 - INFO - Found 144 photos
2025-12-01 22:15:01 - INFO - ✓ Updated _DSC9759.ARW with GPS: 20.212266, 110.986642
...
============================================================
SUMMARY
============================================================
Total photos:              144
Updated:                   120
Skipped (has GPS):         0
Skipped (no timestamp):    0
Skipped (no GPS match):    24
```

## Edge Cases Handled

1. **Photos outside GPS trace time range**: Matched to nearest trackpoint if within max time difference
2. **Photos with existing GPS data**: Skipped by default unless `--overwrite-gps` is used
3. **Photos without timestamps**: Skipped with warning
4. **No matching GPS trackpoints**: Skipped with warning
5. **Timezone considerations**: All times treated as UTC; you can shift photo times with `--time-offset-hours` to align camera time with GPX time

## Limitations

- Photo timestamps must be in EXIF data
- GPS trace must cover (approximately) the same time period as photos
- RAW file writing may not work for all camera models (tested with Sony ARW)
- Interpolation assumes linear movement between trackpoints
