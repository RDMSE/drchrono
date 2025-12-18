# Groups Configuration

## Overview
The application supports loading groups from two different sources:
1. XLSX file (preferred method)
2. INI file (fallback method)

## Loading Groups from XLSX File

### File Format
Create a file named `groups.xlsx` in the application directory with the following structure:

| Cell | Content | Description |
|------|---------|-------------|
| A1 | `Trial: <trial_name>` | Trial name prefixed with "Trial: " |
| A2 | `Groups` | Header (optional but recommended) |
| A3+ | Group names | One group name per row |

### Example File Structure
```
A1: Trial: DR EXTREMO 2025
A2: Groups
A3: Pro
A4: Sport
A5: Tourism
A6: Elite
A7: Amateur
```

### Usage
1. Place the `groups.xlsx` file in the same directory as the application executable
2. Launch the application
3. The groups will be automatically loaded from the XLSX file
4. Groups are stored in the database and associated with the trial name

### Fallback to INI File
If `groups.xlsx` is not found, the application will load groups from `settings.ini` using the existing format:
```ini
[General]
Groups="Pro, Sport, Tourism"
Trial="DR EXTREMO 2025"
```

## Creating a Sample XLSX File

A Python script is provided to create a sample `groups.xlsx` file:

```bash
python3 create_sample_xlsx.py
```

The script requires the `openpyxl` library:
```bash
pip3 install openpyxl
```

## Database Storage
Groups loaded from either source are stored in the `groups` table in the SQLite database (`cronometro.db`) and linked to the trial via `trialId`.

## Notes
- Duplicate groups are automatically ignored when loading
- Empty rows in the XLSX file are skipped
- The trial name must match the format `Trial: <name>` in cell A1
- Groups are loaded only once when starting a new trial
- Existing trials will use previously loaded groups from the database
