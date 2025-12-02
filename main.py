#!/usr/bin/env python3
"""
GPS Photo Geotagging Script

Adds GPS coordinates to photos based on GPX trace files by matching timestamps.
"""

import argparse
import sys
from pathlib import Path
from datetime import datetime, timezone, timedelta
from typing import List, Tuple, Optional, Dict, Any
import logging
import subprocess
import json
from concurrent.futures import ThreadPoolExecutor, as_completed
import os

import gpxpy
import gpxpy.gpx
import piexif


# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


class GPSTrackPoint:
    """Represents a GPS trackpoint with timestamp and coordinates."""
    
    def __init__(self, timestamp: datetime, latitude: float, longitude: float, elevation: Optional[float] = None):
        self.timestamp = timestamp
        self.latitude = latitude
        self.longitude = longitude
        self.elevation = elevation
    
    def __repr__(self):
        return f"GPSTrackPoint({self.timestamp}, {self.latitude}, {self.longitude}, {self.elevation}m)"


class GPXParser:
    """Parser for GPX trace files."""
    
    def __init__(self, gpx_file: Path):
        self.gpx_file = gpx_file
        self.trackpoints: List[GPSTrackPoint] = []
    
    def parse(self) -> List[GPSTrackPoint]:
        """Parse GPX file and extract trackpoints."""
        logger.info(f"Parsing GPX file: {self.gpx_file}")
        
        with open(self.gpx_file, 'r', encoding='utf-8') as f:
            gpx = gpxpy.parse(f)
        
        for track in gpx.tracks:
            for segment in track.segments:
                for point in segment.points:
                    # Ensure timezone-aware datetime
                    timestamp = point.time
                    if timestamp.tzinfo is None:
                        timestamp = timestamp.replace(tzinfo=timezone.utc)
                    
                    trackpoint = GPSTrackPoint(
                        timestamp=timestamp,
                        latitude=point.latitude,
                        longitude=point.longitude,
                        elevation=point.elevation
                    )
                    self.trackpoints.append(trackpoint)
        
        # Sort by timestamp
        self.trackpoints.sort(key=lambda p: p.timestamp)
        
        logger.info(f"Parsed {len(self.trackpoints)} trackpoints")
        if self.trackpoints:
            logger.info(f"Time range: {self.trackpoints[0].timestamp} to {self.trackpoints[-1].timestamp}")
        
        return self.trackpoints
    
    def calculate_average_interval(self) -> float:
        """Calculate average time interval between trackpoints in seconds."""
        if len(self.trackpoints) < 2:
            return 300.0  # Default 5 minutes
        
        intervals = []
        for i in range(1, len(self.trackpoints)):
            delta = (self.trackpoints[i].timestamp - self.trackpoints[i-1].timestamp).total_seconds()
            if delta > 0:  # Ignore zero intervals
                intervals.append(delta)
        
        if not intervals:
            return 300.0
        
        avg_interval = sum(intervals) / len(intervals)
        logger.info(f"Average trackpoint interval: {avg_interval:.1f} seconds")
        return avg_interval


class PhotoProcessor:
    """Handles reading and writing GPS data to photo EXIF."""
    
    SUPPORTED_EXTENSIONS = {
        # JPEG formats
        '.jpg', '.jpeg',
        # RAW formats
        '.arw', '.nef', '.cr2', '.dng', '.orf', '.rw2', '.raf', '.raw',
        # Modern formats
        '.avif', '.heic', '.heif', '.webp',
        # Other common formats
        '.tiff', '.tif', '.png'
    }
    
    def __init__(self, photo_path: Path, time_offset_seconds: float = 0.0):
        self.photo_path = photo_path
        self.is_raw = photo_path.suffix.lower() not in {'.jpg', '.jpeg'}
        # Camera timezone offset from UTC, in seconds (e.g. +8h for Asia/Shanghai).
        # Positive = camera clock is AHEAD of UTC, negative = behind.
        # We convert local EXIF time to UTC by SUBTRACTING this offset.
        self.time_offset_seconds = time_offset_seconds
        # Lazy metadata caches to avoid repeated disk / exiftool work
        self._raw_info: Optional[Dict] = None
        self._jpeg_exif_dict: Optional[Dict] = None
    
    def get_photo_timestamp(self) -> Optional[datetime]:
        """Extract photo capture timestamp from EXIF."""
        try:
            if self.is_raw:
                # For RAW files, use ExifTool, which has robust RAW support
                info = self._get_raw_info()
                if info is None:
                    return None

                # Candidate date/time tags in order of preference
                candidates = [
                    "EXIF:DateTimeOriginal",
                    "EXIF:CreateDate",
                    "QuickTime:CreateDate",
                    "Composite:SubSecDateTimeOriginal",
                ]

                for tag in candidates:
                    value = info.get(tag)
                    if not value:
                        continue

                    # Strip subseconds if present (e.g. "2025:01:02 03:04:05.123")
                    value_str = str(value).split(".")[0]
                    try:
                        dt = datetime.strptime(value_str, "%Y:%m:%d %H:%M:%S")
                    except ValueError:
                        # Try next candidate
                        continue

                    # EXIF times are local camera time; convert to UTC using camera offset
                    dt = dt.replace(tzinfo=timezone.utc)
                    if self.time_offset_seconds:
                        dt = dt - timedelta(seconds=self.time_offset_seconds)
                    return dt
            else:
                # For JPG files, use piexif
                exif_dict = self._get_jpeg_exif_dict()
                exif_ifd = exif_dict.get('Exif', {})
                
                # Try DateTimeOriginal first, then DateTime
                timestamp_bytes = exif_ifd.get(piexif.ExifIFD.DateTimeOriginal) or \
                                 exif_ifd.get(piexif.ExifIFD.DateTime)
                
                if timestamp_bytes:
                    timestamp_str = timestamp_bytes.decode('utf-8')
                    dt = datetime.strptime(timestamp_str, "%Y:%m:%d %H:%M:%S")
                    # EXIF times are local camera time; convert to UTC using camera offset
                    dt = dt.replace(tzinfo=timezone.utc)
                    if self.time_offset_seconds:
                        dt = dt - timedelta(seconds=self.time_offset_seconds)
                    return dt
        
        except Exception as e:
            logger.warning(f"Failed to read timestamp from {self.photo_path.name}: {e}")
        
        return None
    
    def has_gps_data(self) -> bool:
        """Check if photo already has GPS data."""
        try:
            if self.is_raw:
                info = self._get_raw_info()
                if info is None:
                    return False
                lat = info.get("EXIF:GPSLatitude") or info.get("Composite:GPSLatitude")
                lon = info.get("EXIF:GPSLongitude") or info.get("Composite:GPSLongitude")
                return lat is not None and lon is not None
            else:
                exif_dict = self._get_jpeg_exif_dict()
                gps_ifd = exif_dict.get('GPS', {})
                return piexif.GPSIFD.GPSLatitude in gps_ifd
        except Exception:
            return False
    
    def write_gps_data(self, latitude: float, longitude: float, elevation: Optional[float] = None, dry_run: bool = False):
        """Write GPS coordinates to photo EXIF."""
        if dry_run:
            logger.info(f"[DRY RUN] Would write GPS to {self.photo_path.name}: {latitude:.6f}, {longitude:.6f}")
            return
        
        try:
            if self.is_raw:
                self._write_gps_to_raw(latitude, longitude, elevation)
            else:
                self._write_gps_to_jpg(latitude, longitude, elevation)
            
            logger.info(f"✓ Updated {self.photo_path.name} with GPS: {latitude:.6f}, {longitude:.6f}")
        
        except Exception as e:
            logger.error(f"Failed to write GPS to {self.photo_path.name}: {e}")
    
    def _write_gps_to_jpg(self, latitude: float, longitude: float, elevation: Optional[float] = None):
        """Write GPS data to JPG file using piexif."""
        exif_dict = self._get_jpeg_exif_dict()
        
        # Convert decimal degrees to degrees, minutes, seconds
        lat_deg, lat_min, lat_sec = self._decimal_to_dms(abs(latitude))
        lon_deg, lon_min, lon_sec = self._decimal_to_dms(abs(longitude))
        
        # Create GPS IFD
        gps_ifd = {
            piexif.GPSIFD.GPSLatitudeRef: 'N' if latitude >= 0 else 'S',
            piexif.GPSIFD.GPSLatitude: ((lat_deg, 1), (lat_min, 1), (int(lat_sec * 100), 100)),
            piexif.GPSIFD.GPSLongitudeRef: 'E' if longitude >= 0 else 'W',
            piexif.GPSIFD.GPSLongitude: ((lon_deg, 1), (lon_min, 1), (int(lon_sec * 100), 100)),
        }
        
        if elevation is not None:
            gps_ifd[piexif.GPSIFD.GPSAltitudeRef] = 0 if elevation >= 0 else 1
            gps_ifd[piexif.GPSIFD.GPSAltitude] = (int(abs(elevation) * 100), 100)
        
        exif_dict['GPS'] = gps_ifd
        
        # Write back to file
        exif_bytes = piexif.dump(exif_dict)
        piexif.insert(exif_bytes, str(self.photo_path))
        # Invalidate cache so subsequent reads see updated GPS
        self._jpeg_exif_dict = None
    
    def _write_gps_to_raw(self, latitude: float, longitude: float, elevation: Optional[float] = None):
        """Write GPS data to RAW file using ExifTool."""
        cmd = ["exiftool", "-overwrite_original",
               f"-GPSLatitude={latitude}",
               "-GPSLatitudeRef=" + ("N" if latitude >= 0 else "S"),
               f"-GPSLongitude={longitude}",
               "-GPSLongitudeRef=" + ("E" if longitude >= 0 else "W")]

        if elevation is not None:
            cmd.append(f"-GPSAltitude={elevation}")
            cmd.append("-GPSAltitudeRef=0" if elevation >= 0 else "-GPSAltitudeRef=1")

        cmd.append(str(self.photo_path))

        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        if result.returncode != 0:
            raise RuntimeError(f"ExifTool error while writing GPS to {self.photo_path.name}: {result.stderr.strip()}")

    def _run_exiftool_json(self) -> Optional[Dict]:
        """
        Run exiftool in JSON mode on this file and return the first record as a dict.
        Returns None if exiftool is not available or an error occurs.
        """
        try:
            result = subprocess.run(
                ["exiftool", "-j", "-G", "-n", str(self.photo_path)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                text=True,
            )
        except FileNotFoundError:
            logger.error("ExifTool is not installed or not found in PATH; RAW files will not be processed reliably.")
            return None

        if result.returncode != 0:
            logger.warning(f"ExifTool failed for {self.photo_path.name}: {result.stderr.strip()}")
            return None

        try:
            data = json.loads(result.stdout)
            if not data:
                return None
            return data[0]
        except json.JSONDecodeError as e:
            logger.warning(f"Failed to parse ExifTool JSON output for {self.photo_path.name}: {e}")
            return None
    
    def _get_raw_info(self) -> Optional[Dict]:
        """Cached wrapper around ExifTool JSON for RAW files."""
        if self._raw_info is None:
            self._raw_info = self._run_exiftool_json()
        return self._raw_info

    def _get_jpeg_exif_dict(self) -> Dict:
        """Cached wrapper around piexif.load for JPEG files."""
        if self._jpeg_exif_dict is None:
            self._jpeg_exif_dict = piexif.load(str(self.photo_path))
        return self._jpeg_exif_dict
    
    @staticmethod
    def _decimal_to_dms(decimal: float) -> Tuple[int, int, float]:
        """Convert decimal degrees to degrees, minutes, seconds."""
        degrees = int(decimal)
        minutes_decimal = (decimal - degrees) * 60
        minutes = int(minutes_decimal)
        seconds = (minutes_decimal - minutes) * 60
        return degrees, minutes, seconds
    
    @staticmethod
    def _decimal_to_dms_tuple(decimal: float) -> Tuple[float, float, float]:
        """Convert decimal degrees to tuple of (degrees, minutes, seconds)."""
        degrees = int(decimal)
        minutes_decimal = (decimal - degrees) * 60
        minutes = int(minutes_decimal)
        seconds = (minutes_decimal - minutes) * 60
        return (float(degrees), float(minutes), seconds)


class GPSMatcher:
    """Matches photo timestamps with GPS trackpoints."""
    
    def __init__(self, trackpoints: List[GPSTrackPoint], max_time_diff: float, force_interpolate: bool = False):
        self.trackpoints = trackpoints
        self.max_time_diff = max_time_diff
        self.force_interpolate = force_interpolate
    
    def find_gps_for_photo(self, photo_time: datetime) -> Optional[Tuple[float, float, Optional[float]]]:
        """
        Find GPS coordinates for a photo timestamp.
        Returns (latitude, longitude, elevation) or None if no match found.
        """
        if not self.trackpoints:
            return None
        
        # Find the two closest trackpoints
        before = None
        after = None
        
        for i, point in enumerate(self.trackpoints):
            if point.timestamp <= photo_time:
                before = point
            else:
                after = point
                break
        
        # Check if photo is outside the GPS track time range
        if before is None:
            # Photo is before first trackpoint
            time_diff = (self.trackpoints[0].timestamp - photo_time).total_seconds()
            if self.force_interpolate or time_diff <= self.max_time_diff:
                return (self.trackpoints[0].latitude, self.trackpoints[0].longitude, self.trackpoints[0].elevation)
            return None
        
        if after is None:
            # Photo is after last trackpoint
            time_diff = (photo_time - self.trackpoints[-1].timestamp).total_seconds()
            if self.force_interpolate or time_diff <= self.max_time_diff:
                return (self.trackpoints[-1].latitude, self.trackpoints[-1].longitude, self.trackpoints[-1].elevation)
            return None
        
        # Interpolate between before and after
        time_diff_before = (photo_time - before.timestamp).total_seconds()
        time_diff_after = (after.timestamp - photo_time).total_seconds()
        
        # Check if within acceptable time range
        if not self.force_interpolate and min(time_diff_before, time_diff_after) > self.max_time_diff:
            return None
        
        # Linear interpolation
        total_time = (after.timestamp - before.timestamp).total_seconds()
        if total_time == 0:
            # Exact match or very close points
            return (before.latitude, before.longitude, before.elevation)
        
        ratio = time_diff_before / total_time
        
        latitude = before.latitude + (after.latitude - before.latitude) * ratio
        longitude = before.longitude + (after.longitude - before.longitude) * ratio
        
        elevation = None
        if before.elevation is not None and after.elevation is not None:
            elevation = before.elevation + (after.elevation - before.elevation) * ratio
        
        return (latitude, longitude, elevation)


def find_photos(directory: Path, recursive: bool = False) -> List[Path]:
    """Find all supported photo files in directory."""
    photos = []
    
    if recursive:
        for ext in PhotoProcessor.SUPPORTED_EXTENSIONS:
            photos.extend(directory.rglob(f'*{ext}'))
            photos.extend(directory.rglob(f'*{ext.upper()}'))
    else:
        for ext in PhotoProcessor.SUPPORTED_EXTENSIONS:
            photos.extend(directory.glob(f'*{ext}'))
            photos.extend(directory.glob(f'*{ext.upper()}'))
    
    return sorted(photos)


def process_photos(
    gpx_file: Path,
    photos_dir: Path,
    overwrite_gps: bool = False,
    dry_run: bool = False,
    max_time_diff: Optional[float] = None,
    recursive: bool = False,
    time_offset_seconds: float = 0.0,
    workers: int = 0,
    force_interpolate: bool = False,
):
    """Main processing function."""
    
    # Parse GPX file
    parser = GPXParser(gpx_file)
    trackpoints = parser.parse()
    
    if not trackpoints:
        logger.error("No trackpoints found in GPX file")
        return
    
    # Determine max time difference if not specified
    if max_time_diff is None:
        avg_interval = parser.calculate_average_interval()
        # Use 3x the average interval as max, with reasonable bounds
        max_time_diff = max(60, min(avg_interval * 3, 600))
        logger.info(f"Using adaptive max time difference: {max_time_diff:.0f} seconds")
    else:
        logger.info(f"Using specified max time difference: {max_time_diff:.0f} seconds")
    
    # Find photos
    photos = find_photos(photos_dir, recursive)
    logger.info(f"Found {len(photos)} photos")
    
    if not photos:
        logger.warning("No photos found")
        return
    
    # Create GPS matcher
    matcher = GPSMatcher(trackpoints, max_time_diff, force_interpolate=force_interpolate)
    
    # Process each photo
    stats = {
        'total': len(photos),
        'updated': 0,
        'skipped_has_gps': 0,
        'skipped_no_timestamp': 0,
        'skipped_no_match': 0,
    }
    # Photos whose timestamps fall within the GPX time range but had no GPS match
    skipped_in_range_no_match: List[Tuple[Path, datetime]] = []
    
    def _process_single_photo(photo_path: Path) -> Dict[str, Any]:
        """Worker function for processing a single photo."""
        result: Dict[str, Any] = {
            'updated': 0,
            'skipped_has_gps': 0,
            'skipped_no_timestamp': 0,
            'skipped_no_match': 0,
            'in_range_no_match': None,
        }

        processor = PhotoProcessor(photo_path, time_offset_seconds=time_offset_seconds)

        # Check if photo has GPS data
        if not overwrite_gps and processor.has_gps_data():
            logger.info(f"⊘ Skipping {photo_path.name} (already has GPS data)")
            result['skipped_has_gps'] = 1
            return result

        # Get photo timestamp
        photo_time = processor.get_photo_timestamp()
        if photo_time is None:
            logger.warning(f"⊘ Skipping {photo_path.name} (no timestamp found)")
            result['skipped_no_timestamp'] = 1
            return result

        # Find matching GPS coordinates
        gps_data = matcher.find_gps_for_photo(photo_time)
        if gps_data is None:
            result['skipped_no_match'] = 1
            # Distinguish between outside GPX time range vs within range but beyond max_time_diff
            first_ts = trackpoints[0].timestamp
            last_ts = trackpoints[-1].timestamp
            if first_ts <= photo_time <= last_ts:
                result['in_range_no_match'] = (photo_path, photo_time)
                logger.warning(f"⊘ Skipping {photo_path.name} (no GPS match within max_time_diff for {photo_time})")
            else:
                logger.warning(f"⊘ Skipping {photo_path.name} (photo time {photo_time} outside GPX time range)")
            return result

        # Write GPS data
        latitude, longitude, elevation = gps_data
        processor.write_gps_data(latitude, longitude, elevation, dry_run)
        result['updated'] = 1
        return result

    # Determine number of worker threads for IO-bound parallelism
    if workers <= 0:
        workers = min(32, (os.cpu_count() or 4) * 2)
    if workers <= 1:
        # Fallback to simple sequential processing
        for photo_path in photos:
            res = _process_single_photo(photo_path)
            stats['updated'] += res['updated']
            stats['skipped_has_gps'] += res['skipped_has_gps']
            stats['skipped_no_timestamp'] += res['skipped_no_timestamp']
            stats['skipped_no_match'] += res['skipped_no_match']
            if res['in_range_no_match']:
                skipped_in_range_no_match.append(res['in_range_no_match'])
    else:
        logger.info(f"Processing photos with up to {workers} concurrent workers")
        with ThreadPoolExecutor(max_workers=workers) as executor:
            future_to_photo = {executor.submit(_process_single_photo, p): p for p in photos}
            for future in as_completed(future_to_photo):
                res = future.result()
                stats['updated'] += res['updated']
                stats['skipped_has_gps'] += res['skipped_has_gps']
                stats['skipped_no_timestamp'] += res['skipped_no_timestamp']
                stats['skipped_no_match'] += res['skipped_no_match']
                if res['in_range_no_match']:
                    skipped_in_range_no_match.append(res['in_range_no_match'])
    
    # Print summary
    logger.info("\n" + "="*60)
    logger.info("SUMMARY")
    logger.info("="*60)
    logger.info(f"Total photos:              {stats['total']}")
    logger.info(f"Updated:                   {stats['updated']}")
    logger.info(f"Skipped (has GPS):         {stats['skipped_has_gps']}")
    logger.info(f"Skipped (no timestamp):    {stats['skipped_no_timestamp']}")
    logger.info(f"Skipped (no GPS match):    {stats['skipped_no_match']}")
    
    # Detail photos that were in GPX time range but had no GPS match
    if skipped_in_range_no_match:
        logger.info("\nPhotos within GPX time range but without GPS match (likely too far from any trackpoint):")
        for path, ts in skipped_in_range_no_match:
            logger.info(f"  - {path.name}: {ts}")
    
    if dry_run:
        logger.info("\n[DRY RUN MODE] No files were modified")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Add GPS coordinates to photos based on GPX trace files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Process photos with default settings
  python main.py trace.gpx photos/
  
  # Overwrite existing GPS data and use custom time difference
  python main.py trace.gpx photos/ --overwrite-gps --max-time-diff 120
  
  # Dry run to preview changes
  python main.py trace.gpx photos/ --dry-run
        """
    )
    
    parser.add_argument('gpx_file', type=Path, help='GPX trace file')
    parser.add_argument('photos_dir', type=Path, help='Directory containing photos')
    parser.add_argument('--overwrite-gps', action='store_true',
                       help='Overwrite existing GPS coordinates (default: skip photos with GPS)')
    parser.add_argument('--dry-run', action='store_true',
                       help='Preview changes without modifying files')
    parser.add_argument('--max-time-diff', type=float, metavar='SECONDS',
                       help='Maximum time difference in seconds (default: adaptive based on GPX)')
    parser.add_argument('--recursive', action='store_true',
                       help='Search for photos recursively in subdirectories')
    parser.add_argument(
        '--time-offset-hours',
        type=float,
        default=0.0,
        metavar='HOURS',
        help=(
            'Camera timezone offset from UTC, in hours (e.g. 8 for Asia/Shanghai, -5 for US Eastern Standard Time). '
            'This converts naive EXIF local times to UTC by subtracting the offset before matching with GPX.'
        ),
    )
    parser.add_argument(
        '--workers',
        type=int,
        default=0,
        metavar='N',
        help=(
            'Number of concurrent worker threads to use for processing photos. '
            '0 = auto (IO-bound optimized based on CPU count). 1 = disable parallelism.'
        ),
    )
    parser.add_argument(
        '--force-interpolate',
        action='store_true',
        help=(
            'Always interpolate between the nearest two trackpoints (or use the nearest endpoint) '
            'even when the photo timestamp is farther than --max-time-diff away. '
            'Useful when you want to rely solely on GPX track geometry.'
        ),
    )
    
    args = parser.parse_args()
    
    # Validate inputs
    if not args.gpx_file.exists():
        logger.error(f"GPX file not found: {args.gpx_file}")
        sys.exit(1)
    
    if not args.photos_dir.exists():
        logger.error(f"Photos directory not found: {args.photos_dir}")
        sys.exit(1)
    
    if not args.photos_dir.is_dir():
        logger.error(f"Not a directory: {args.photos_dir}")
        sys.exit(1)
    
    # Process photos
    try:
        time_offset_seconds = args.time_offset_hours * 3600.0
        if time_offset_seconds:
            logger.info(
                f"Assuming camera timezone offset: {args.time_offset_hours:+.2f} hours from UTC "
                f"(converting EXIF local time to UTC before matching GPX)."
            )

        process_photos(
            gpx_file=args.gpx_file,
            photos_dir=args.photos_dir,
            overwrite_gps=args.overwrite_gps,
            dry_run=args.dry_run,
            max_time_diff=args.max_time_diff,
            recursive=args.recursive,
            time_offset_seconds=time_offset_seconds,
            workers=args.workers,
            force_interpolate=args.force_interpolate,
        )
    except KeyboardInterrupt:
        logger.info("\nOperation cancelled by user")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}", exc_info=True)
        sys.exit(1)


if __name__ == "__main__":
    main()
