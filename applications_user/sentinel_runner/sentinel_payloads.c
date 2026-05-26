/**
 * @file sentinel_payloads.c
 * @brief OverL0rk Sentinel — payload definitions (ducky-script strings).
 *
 * Author : Eudys Ramirez (@OverL0rk)
 *
 * Conventions for every payload below:
 *   - Begin with REM lines identifying it as Sentinel + purpose.
 *   - Initial DELAY 1500 gives user time to release the Flipper button
 *     and switch focus to the target host.
 *   - Output file path is hard-coded per OS:
 *       Windows  →  C:\Temp\sentinel-<topic>.txt
 *       Linux    →  $HOME/sentinel-<topic>.txt
 *       macOS    →  $HOME/sentinel-<topic>.txt
 *   - Final command opens the output in a default viewer
 *     (notepad on Win, xdg-open on Linux, open on macOS).
 *   - Window is left open so the operator can re-run if needed.
 *
 * Why per-line ENTER instead of one big STRING with semicolons:
 *   Long STRING commands take ~30ms/char on USB HID.  A 500-char one-liner
 *   becomes 15s of typing — easy to interrupt by accidental keypress.
 *   Per-line commands are interactive: each line is acked by the shell
 *   and the user sees progress.
 */

#include "sentinel_payloads.h"

/* ── Windows: Port Scan ─────────────────────────────────────────────────── */

static const char k_win_portscan[] =
    "REM OverL0rk Sentinel - Port Scan (Windows 10/11)\n"
    "REM Read-only audit: lists LISTEN + ESTABLISHED TCP/UDP ports.\n"
    "REM Output: C:\\Temp\\sentinel-ports.txt (opened in Notepad)\n"
    "DELAY 1500\n"
    "GUI r\n"
    "DELAY 500\n"
    "STRING powershell -NoProfile -ExecutionPolicy Bypass\n"
    "ENTER\n"
    "DELAY 1800\n"
    "STRING $o='C:\\Temp\\sentinel-ports.txt'\n"
    "ENTER\n"
    "STRING New-Item -Force -ItemType Directory C:\\Temp | Out-Null\n"
    "ENTER\n"
    "STRING '== OverL0rk Sentinel - Port Scan ==' | Out-File $o\n"
    "ENTER\n"
    "STRING Get-Date | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- LISTEN TCP --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-NetTCPConnection -State Listen | Sort LocalPort | ft LocalAddress,LocalPort,OwningProcess -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- ESTABLISHED TCP --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-NetTCPConnection -State Established | Sort RemoteAddress | ft LocalPort,RemoteAddress,RemotePort,OwningProcess -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- LISTEN UDP --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-NetUDPEndpoint | Sort LocalPort | ft LocalAddress,LocalPort,OwningProcess -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING notepad $o\n"
    "ENTER\n"
    "DELAY 300\n"
    "STRING exit\n"
    "ENTER\n";

/* ── Windows: AV / Defender Check ──────────────────────────────────────── */

static const char k_win_avcheck[] =
    "REM OverL0rk Sentinel - AV Check (Windows 10/11)\n"
    "REM Reads Windows Defender status + last threat detections + 3rd-party AV.\n"
    "REM Output: C:\\Temp\\sentinel-av.txt\n"
    "DELAY 1500\n"
    "GUI r\n"
    "DELAY 500\n"
    "STRING powershell -NoProfile -ExecutionPolicy Bypass\n"
    "ENTER\n"
    "DELAY 1800\n"
    "STRING $o='C:\\Temp\\sentinel-av.txt'\n"
    "ENTER\n"
    "STRING New-Item -Force -ItemType Directory C:\\Temp | Out-Null\n"
    "ENTER\n"
    "STRING '== OverL0rk Sentinel - AV Check ==' | Out-File $o\n"
    "ENTER\n"
    "STRING Get-Date | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Defender computer status --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { Get-MpComputerStatus | Format-List | Out-File $o -Append } catch { 'Defender unavailable' | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '-- Recent threat detections --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { Get-MpThreatDetection | Format-List | Out-File $o -Append } catch { 'No detections or cmdlet unavailable' | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '-- Registered antivirus products --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { Get-CimInstance -Namespace root\\SecurityCenter2 -ClassName AntivirusProduct | Format-List displayName,productState,timestamp | Out-File $o -Append } catch { 'SecurityCenter2 query failed' | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '-- Defender signature versions --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { Get-MpComputerStatus | Select AntivirusSignatureVersion,AntivirusSignatureLastUpdated,NISSignatureVersion | Format-List | Out-File $o -Append } catch {}\n"
    "ENTER\n"
    "STRING notepad $o\n"
    "ENTER\n"
    "DELAY 300\n"
    "STRING exit\n"
    "ENTER\n";

/* ── Windows: Process Audit ────────────────────────────────────────────── */

static const char k_win_procaudit[] =
    "REM OverL0rk Sentinel - Process Audit (Windows 10/11)\n"
    "REM Top 20 processes by CPU and memory + unsigned binary check.\n"
    "REM Output: C:\\Temp\\sentinel-proc.txt\n"
    "DELAY 1500\n"
    "GUI r\n"
    "DELAY 500\n"
    "STRING powershell -NoProfile -ExecutionPolicy Bypass\n"
    "ENTER\n"
    "DELAY 1800\n"
    "STRING $o='C:\\Temp\\sentinel-proc.txt'\n"
    "ENTER\n"
    "STRING New-Item -Force -ItemType Directory C:\\Temp | Out-Null\n"
    "ENTER\n"
    "STRING '== OverL0rk Sentinel - Process Audit ==' | Out-File $o\n"
    "ENTER\n"
    "STRING Get-Date | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Top 20 by CPU time --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-Process | Sort CPU -Desc | Select -First 20 Name,Id,CPU,WS,Path | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Top 20 by WorkingSet (RAM) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-Process | Sort WS -Desc | Select -First 20 Name,Id,WS,Path | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Processes with unsigned or invalid binaries --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-Process | Where { $_.Path } | ForEach { try { $s=Get-AuthenticodeSignature $_.Path -ErrorAction Stop; if($s.Status -ne 'Valid'){[pscustomobject]@{Name=$_.Name;Status=$s.Status;Path=$_.Path}} } catch {} } | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Processes with network sockets (top 30) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-NetTCPConnection -State Established | Group OwningProcess | Sort Count -Desc | Select -First 30 @{n='PID';e={$_.Name}},Count | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING notepad $o\n"
    "ENTER\n"
    "DELAY 300\n"
    "STRING exit\n"
    "ENTER\n";

/* ── Windows: Network Audit ────────────────────────────────────────────── */

static const char k_win_netaudit[] =
    "REM OverL0rk Sentinel - Network Audit (Windows 10/11)\n"
    "REM Firewall, ARP table, DNS cache, wireless profiles.\n"
    "REM Output: C:\\Temp\\sentinel-net.txt\n"
    "DELAY 1500\n"
    "GUI r\n"
    "DELAY 500\n"
    "STRING powershell -NoProfile -ExecutionPolicy Bypass\n"
    "ENTER\n"
    "DELAY 1800\n"
    "STRING $o='C:\\Temp\\sentinel-net.txt'\n"
    "ENTER\n"
    "STRING New-Item -Force -ItemType Directory C:\\Temp | Out-Null\n"
    "ENTER\n"
    "STRING '== OverL0rk Sentinel - Network Audit ==' | Out-File $o\n"
    "ENTER\n"
    "STRING Get-Date | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Firewall profiles --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { Get-NetFirewallProfile | ft Name,Enabled,DefaultInboundAction,DefaultOutboundAction -A | Out-File $o -Append } catch { 'Firewall query failed' | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '-- ARP table (reachable neighbors) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-NetNeighbor -State Reachable | ft IPAddress,LinkLayerAddress,InterfaceAlias -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- DNS client cache (last 40) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-DnsClientCache | Select -Last 40 | ft Entry,Data,RecordType -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Wireless profiles (saved SSIDs) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING netsh wlan show profiles | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '-- Routing table --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-NetRoute -AddressFamily IPv4 | ft DestinationPrefix,NextHop,InterfaceAlias,RouteMetric -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING notepad $o\n"
    "ENTER\n"
    "DELAY 300\n"
    "STRING exit\n"
    "ENTER\n";

/* ── Windows: System Binary Hash Check ────────────────────────────────── */

static const char k_win_hashcheck[] =
    "REM OverL0rk Sentinel - System Binary Hashes (Windows 10/11)\n"
    "REM SHA-256 of critical Windows binaries.  Compare against a known-good baseline.\n"
    "REM Output: C:\\Temp\\sentinel-hash.txt\n"
    "DELAY 1500\n"
    "GUI r\n"
    "DELAY 500\n"
    "STRING powershell -NoProfile -ExecutionPolicy Bypass\n"
    "ENTER\n"
    "DELAY 1800\n"
    "STRING $o='C:\\Temp\\sentinel-hash.txt'\n"
    "ENTER\n"
    "STRING New-Item -Force -ItemType Directory C:\\Temp | Out-Null\n"
    "ENTER\n"
    "STRING '== OverL0rk Sentinel - Critical Binary Hashes ==' | Out-File $o\n"
    "ENTER\n"
    "STRING Get-Date | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $files=@('C:\\Windows\\System32\\cmd.exe','C:\\Windows\\System32\\notepad.exe','C:\\Windows\\explorer.exe','C:\\Windows\\System32\\svchost.exe','C:\\Windows\\System32\\lsass.exe','C:\\Windows\\System32\\winlogon.exe','C:\\Windows\\System32\\spoolsv.exe','C:\\Windows\\System32\\services.exe')\n"
    "ENTER\n"
    "STRING foreach($f in $files){ if(Test-Path $f){ Get-FileHash $f -Algorithm SHA256 | Out-File $o -Append } else { ($f + ' NOT FOUND') | Out-File $o -Append } }\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING 'Compare these hashes against Microsoft baseline or a previously trusted snapshot.' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING 'Mismatch = potential tampering or malware.' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING notepad $o\n"
    "ENTER\n"
    "DELAY 300\n"
    "STRING exit\n"
    "ENTER\n";

/* ── Linux: Port Audit ────────────────────────────────────────────────── */

static const char k_linux_portaudit[] =
    "REM OverL0rk Sentinel - Port Audit (Linux)\n"
    "REM Requires ss (iproute2).  Output: ~/sentinel-ports.txt\n"
    "REM Opens a terminal via Ctrl+Alt+T (Ubuntu/Mint/Fedora default).\n"
    "DELAY 1500\n"
    "CTRL ALT t\n"
    "DELAY 2500\n"
    "STRING out=~/sentinel-ports.txt\n"
    "ENTER\n"
    "STRING echo '== OverL0rk Sentinel - Port Audit ==' > $out\n"
    "ENTER\n"
    "STRING date >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- LISTEN ports (ss -tulpn) --' >> $out\n"
    "ENTER\n"
    "STRING ss -tulpn 2>/dev/null | sort >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- ESTABLISHED TCP --' >> $out\n"
    "ENTER\n"
    "STRING ss -t state established 2>/dev/null >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- Listening processes (lsof) --' >> $out\n"
    "ENTER\n"
    "STRING lsof -iTCP -sTCP:LISTEN -n -P 2>/dev/null >> $out || echo 'lsof not available without privileges' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- iptables rules (if accessible) --' >> $out\n"
    "ENTER\n"
    "STRING (iptables -L -n 2>/dev/null || echo 'iptables requires root') >> $out\n"
    "ENTER\n"
    "STRING xdg-open $out 2>/dev/null || less $out\n"
    "ENTER\n";

/* ── Linux: Process Audit ─────────────────────────────────────────────── */

static const char k_linux_procaudit[] =
    "REM OverL0rk Sentinel - Process Audit (Linux)\n"
    "REM Process tree + processes with network connections.\n"
    "REM Output: ~/sentinel-proc.txt\n"
    "DELAY 1500\n"
    "CTRL ALT t\n"
    "DELAY 2500\n"
    "STRING out=~/sentinel-proc.txt\n"
    "ENTER\n"
    "STRING echo '== OverL0rk Sentinel - Process Audit ==' > $out\n"
    "ENTER\n"
    "STRING date >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- Process tree (ps auxf) --' >> $out\n"
    "ENTER\n"
    "STRING ps auxf >> $out 2>&1\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- Top 15 by CPU --' >> $out\n"
    "ENTER\n"
    "STRING ps aux --sort=-%cpu | head -16 >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- Top 15 by RAM --' >> $out\n"
    "ENTER\n"
    "STRING ps aux --sort=-%mem | head -16 >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- Processes with TCP/UDP sockets --' >> $out\n"
    "ENTER\n"
    "STRING ss -tup 2>/dev/null | tail -n +2 | sort -u >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- systemd failed units --' >> $out\n"
    "ENTER\n"
    "STRING systemctl --failed --no-pager 2>/dev/null >> $out\n"
    "ENTER\n"
    "STRING xdg-open $out 2>/dev/null || less $out\n"
    "ENTER\n";

/* ── macOS: Port Audit ────────────────────────────────────────────────── */

static const char k_mac_portaudit[] =
    "REM OverL0rk Sentinel - Port Audit (macOS)\n"
    "REM Output: ~/sentinel-ports.txt (opened with TextEdit).\n"
    "REM Opens Terminal via Spotlight (Cmd+Space).\n"
    "DELAY 1500\n"
    "GUI SPACE\n"
    "DELAY 700\n"
    "STRING terminal\n"
    "DELAY 700\n"
    "ENTER\n"
    "DELAY 2500\n"
    "STRING out=~/sentinel-ports.txt\n"
    "ENTER\n"
    "STRING echo '== OverL0rk Sentinel - Port Audit (macOS) ==' > $out\n"
    "ENTER\n"
    "STRING date >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- LISTEN TCP/UDP (lsof) --' >> $out\n"
    "ENTER\n"
    "STRING lsof -i -P -n 2>/dev/null | grep LISTEN >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- ESTABLISHED connections --' >> $out\n"
    "ENTER\n"
    "STRING lsof -i -P -n 2>/dev/null | grep ESTABLISHED >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- pf firewall rules (if accessible) --' >> $out\n"
    "ENTER\n"
    "STRING (sudo -n pfctl -s rules 2>/dev/null || echo 'pfctl requires sudo') >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- Listening LaunchDaemons --' >> $out\n"
    "ENTER\n"
    "STRING launchctl list 2>/dev/null | head -50 >> $out\n"
    "ENTER\n"
    "STRING open -a TextEdit $out\n"
    "ENTER\n";

/* ══════════════════════════════════════════════════════════════════════════
 * MALWARE DETECTION PAYLOADS
 *
 * The following payloads detect Indicators of Compromise (IOCs) for the
 * most common malware families on each platform.  Sources:
 *   - MITRE ATT&CK techniques (T1003, T1547, T1053, T1546, T1059, …)
 *   - CISA #StopRansomware advisories
 *   - Mandiant APT reports (Cobalt Strike, Mimikatz, AgentTesla)
 *   - Public IOC feeds (abuse.ch, malwarebazaar)
 *
 * SAFETY MODEL — these payloads are DETECT-ONLY:
 *   - Zero file deletion, zero process termination, zero registry edits.
 *   - Findings are written to a plaintext report; each finding includes a
 *     "# REMEDIATION:" comment with the suggested cleanup command.
 *   - The auditor reviews the report and decides what to execute manually.
 *
 * A heuristic-based auto-remediator is dangerous: legitimate software
 * (corporate AV exclusions, signed-but-misnamed binaries, vendor tools in
 * %APPDATA%) would be destroyed.  Sentinel deliberately keeps the human
 * in the loop.
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Windows: Malware IOC Scan (top 10 families) ──────────────────────── */

static const char k_win_malware_scan[] =
    "REM OverL0rk Sentinel - Malware IOC Scan (Windows 10/11)\n"
    "REM Detects IOCs of top 10 Windows malware families: Mimikatz,\n"
    "REM Cobalt Strike, AgentTesla, Qakbot, Emotet, NjRAT, AsyncRAT,\n"
    "REM XMRig/miners, info-stealers, ransomware staging.\n"
    "REM Output: C:\\Temp\\sentinel-malware.txt + remediation notes inline.\n"
    "DELAY 1500\n"
    "GUI r\n"
    "DELAY 500\n"
    "STRING powershell -NoProfile -ExecutionPolicy Bypass\n"
    "ENTER\n"
    "DELAY 1800\n"
    "STRING $o='C:\\Temp\\sentinel-malware.txt'\n"
    "ENTER\n"
    "STRING New-Item -Force -ItemType Directory C:\\Temp | Out-Null\n"
    "ENTER\n"
    "STRING '== OverL0rk Sentinel - Malware IOC Scan ==' | Out-File $o\n"
    "ENTER\n"
    "STRING Get-Date | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $found=0\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [1] Known malware process names --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $bad=@('mimikatz','procdump64','wce','pwdump','gsecdump','lazagne','meterpreter','beacon','cobaltstrike','xmrig','minerd','kdevtmpfsi','plink','rclone','ngrok','psexec','nc','ncat','frpc','frps')\n"
    "ENTER\n"
    "STRING $hit=Get-Process | Where { $bad -contains $_.Name.ToLower() }; if($hit){$found+=$hit.Count; $hit | Select Name,Id,Path | ft -A | Out-File $o -Append; '# REMEDIATION: Stop-Process -Id <pid> -Force ; investigate parent process via Get-CimInstance Win32_Process' | Out-File $o -Append} else {'OK - none of the known malware names are running' | Out-File $o -Append}\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [2] Persistence: Run/RunOnce registry keys --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $rk=@('HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Run','HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Run','HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce','HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce')\n"
    "ENTER\n"
    "STRING foreach($k in $rk){$v=Get-ItemProperty $k -EA SilentlyContinue; if($v){$v.PSObject.Properties | Where {$_.Name -notmatch '^PS'} | ft Name,Value -A | Out-File $o -Append; $k | Out-File $o -Append}}\n"
    "ENTER\n"
    "STRING '# REMEDIATION: Remove-ItemProperty -Path <key> -Name <value>  (only after confirming it is malicious)' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [3] Services with non-standard paths --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-CimInstance Win32_Service | Where { $_.PathName -match 'AppData|\\\\Temp\\\\|Users\\\\Public' -and $_.PathName -notmatch 'Program Files' } | Select Name,DisplayName,PathName,State | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '# REMEDIATION: Stop-Service <name>; Set-Service <name> -StartupType Disabled; sc.exe delete <name>' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [4] Scheduled tasks created last 7 days (non-Microsoft) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-ScheduledTask | Where { $_.Date -gt (Get-Date).AddDays(-7) -and $_.TaskPath -notlike '\\Microsoft\\*' } | Select TaskName,TaskPath,Date,State | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '# REMEDIATION: Unregister-ScheduledTask -TaskName <name> -Confirm:$false' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [5] Windows Defender exclusions (attacker often disables AV here) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { $p=Get-MpPreference; 'ExclusionPath:' | Out-File $o -Append; $p.ExclusionPath | Out-File $o -Append; 'ExclusionProcess:' | Out-File $o -Append; $p.ExclusionProcess | Out-File $o -Append; 'ExclusionExtension:' | Out-File $o -Append; $p.ExclusionExtension | Out-File $o -Append } catch { 'Defender not accessible' | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '# REMEDIATION: Remove-MpPreference -ExclusionPath <path>  (verify legitimacy first)' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [6] AMSI / script-block-logging tampering --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $amsi=Get-ItemProperty 'HKLM:\\SOFTWARE\\Microsoft\\Windows Script\\Settings' -EA SilentlyContinue; if($amsi.AmsiEnable -eq 0){'CRITICAL: AMSI disabled in registry' | Out-File $o -Append} else {'OK - AMSI not disabled at HKLM' | Out-File $o -Append}\n"
    "ENTER\n"
    "STRING $sbl=Get-ItemProperty 'HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\PowerShell\\ScriptBlockLogging' -EA SilentlyContinue; if(!$sbl -or $sbl.EnableScriptBlockLogging -ne 1){'INFO: PowerShell ScriptBlock logging is OFF (recommend ON for forensics)' | Out-File $o -Append}\n"
    "ENTER\n"
    "STRING '# REMEDIATION (AMSI): Remove-ItemProperty -Path HKLM:\\SOFTWARE\\Microsoft\\\"Windows Script\"\\Settings -Name AmsiEnable' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [7] Files in user/system Startup folders --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-ChildItem \"$env:APPDATA\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\" -EA SilentlyContinue | Select Name,LastWriteTime | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-ChildItem \"$env:ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\" -EA SilentlyContinue | Select Name,LastWriteTime | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [8] Recent executables in %TEMP% (last 3 days) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-ChildItem $env:TEMP,$env:LOCALAPPDATA\\Temp -Include *.exe,*.dll,*.ps1,*.bat,*.vbs,*.scr -Recurse -EA SilentlyContinue | Where { $_.LastWriteTime -gt (Get-Date).AddDays(-3) } | Select FullName,LastWriteTime,Length | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '# REMEDIATION: Get-FileHash <path> -Algorithm SHA256 ; check hash on VirusTotal before deleting' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [9] WMI event subscriptions (advanced persistence) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { $f=Get-WmiObject -Namespace root\\Subscription -Class __EventFilter -EA Stop; if($f){ $f | Select Name,Query | ft -A | Out-File $o -Append } else { 'OK - no WMI event filters' | Out-File $o -Append } } catch { 'WMI subscription query failed (may need elevation)' | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '# REMEDIATION: Get-WmiObject ... -Class __EventFilter | Remove-WmiObject  (advanced; consult IR playbook)' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [10] Recently loaded drivers (unsigned/suspicious) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING Get-CimInstance Win32_SystemDriver | Where { $_.State -eq 'Running' -and $_.PathName -notmatch 'System32|SysWOW64' } | Select Name,PathName,StartMode | ft -A | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '== SCAN COMPLETE ==' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING 'Review each section above.  Cross-check filenames/hashes with VirusTotal.' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING 'NEVER run remediation commands blindly - verify legitimate use first.' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING notepad $o\n"
    "ENTER\n"
    "DELAY 300\n"
    "STRING exit\n"
    "ENTER\n";

/* ── Windows: Ransomware Detection ────────────────────────────────────── */

static const char k_win_ransomware[] =
    "REM OverL0rk Sentinel - Ransomware Detection (Windows 10/11)\n"
    "REM Searches for encrypted file extensions, ransom notes, and verifies\n"
    "REM that Volume Shadow Copies (your recovery lifeline) are intact.\n"
    "REM Output: C:\\Temp\\sentinel-ransom.txt\n"
    "DELAY 1500\n"
    "GUI r\n"
    "DELAY 500\n"
    "STRING powershell -NoProfile -ExecutionPolicy Bypass\n"
    "ENTER\n"
    "DELAY 1800\n"
    "STRING $o='C:\\Temp\\sentinel-ransom.txt'\n"
    "ENTER\n"
    "STRING New-Item -Force -ItemType Directory C:\\Temp | Out-Null\n"
    "ENTER\n"
    "STRING '== OverL0rk Sentinel - Ransomware Check ==' | Out-File $o\n"
    "ENTER\n"
    "STRING Get-Date | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [1] Encrypted file extensions (LockBit/Ryuk/Conti/WannaCry/Cl0p/BlackCat) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $ext=@('*.lockbit','*.lockbit3','*.ryk','*.conti','*.WNCRY','*.encrypted','*.crypt','*.crypted','*.locked','*.cryptolocker','*.cerber','*.zepto','*.osiris','*.clop','*.blackcat','*.bcdedited','*.hive','*.avos','*.lkfr','*.basta')\n"
    "ENTER\n"
    "STRING $scan_dirs=@($env:USERPROFILE,'C:\\Users\\Public','C:\\ProgramData')\n"
    "ENTER\n"
    "STRING foreach($d in $scan_dirs){ Get-ChildItem $d -Include $ext -Recurse -EA SilentlyContinue | Select FullName,LastWriteTime,Length | ft -A | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '# REMEDIATION: If matches found, ISOLATE machine from network immediately.  Do NOT pay ransom.' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [2] Ransom note files --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $notes=@('HOW_TO_DECRYPT*','HOW_TO_RECOVER*','README_TO_DECRYPT*','_readme.txt','_RECOVER*','RESTORE-MY-FILES*','DECRYPT-FILES*','!!READ_ME*','RANSOM*','HELP_DECRYPT*','RECOVERY_INSTRUCTIONS*','LOCKBIT_README*')\n"
    "ENTER\n"
    "STRING foreach($d in $scan_dirs){ Get-ChildItem $d -Include $notes -Recurse -EA SilentlyContinue | Select FullName,LastWriteTime | ft -A | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [3] Volume Shadow Copies (your recovery options) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING try { Get-CimInstance -ClassName Win32_ShadowCopy -EA Stop | Select InstallDate,VolumeName | ft -A | Out-File $o -Append } catch { 'Cannot query shadow copies (need admin)' | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING vssadmin list shadows 2>&1 | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '# IF SHADOWS WERE DELETED: ransomware likely ran vssadmin delete shadows /all /quiet' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [4] Boot configuration tampering (ransomware disables recovery) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING bcdedit | findstr /i 'recoveryenabled bootstatuspolicy' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '# Healthy values: recoveryenabled=Yes, bootstatuspolicy=DisplayAllFailures' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '-- [5] Recently modified .docx/.xlsx/.pdf in user folders (mass encryption sign) --' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING $recent=Get-ChildItem $env:USERPROFILE\\Documents -Include *.docx,*.xlsx,*.pdf -Recurse -EA SilentlyContinue | Where { $_.LastWriteTime -gt (Get-Date).AddHours(-2) } | Measure-Object\n"
    "ENTER\n"
    "STRING if($recent.Count -gt 50){ \"WARNING: $($recent.Count) office files modified in last 2h - possible mass encryption\" | Out-File $o -Append } else { \"OK - only $($recent.Count) office files modified recently\" | Out-File $o -Append }\n"
    "ENTER\n"
    "STRING '' | Out-File $o -Append; '== RANSOMWARE CHECK COMPLETE ==' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING 'If ANY indicators found above: 1) Disconnect from network. 2) Do NOT power off (forensic data in RAM).' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING '3) Photograph ransom note. 4) Check nomoreransom.org for decryptor. 5) Restore from offline backup.' | Out-File $o -Append\n"
    "ENTER\n"
    "STRING notepad $o\n"
    "ENTER\n"
    "DELAY 300\n"
    "STRING exit\n"
    "ENTER\n";

/* ── Linux: Malware Scan ──────────────────────────────────────────────── */

static const char k_linux_malware_scan[] =
    "REM OverL0rk Sentinel - Malware Scan (Linux)\n"
    "REM Detects: XMRig/cryptominers, Diamorphine rootkit, webshells,\n"
    "REM SSH backdoors, ld.so.preload hijacks, suspicious cron, Mirai\n"
    "REM Output: ~/sentinel-malware.txt\n"
    "DELAY 1500\n"
    "CTRL ALT t\n"
    "DELAY 2500\n"
    "STRING out=~/sentinel-malware.txt\n"
    "ENTER\n"
    "STRING echo '== OverL0rk Sentinel - Linux Malware Scan ==' > $out\n"
    "ENTER\n"
    "STRING date >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [1] Known malicious process names --' >> $out\n"
    "ENTER\n"
    "STRING ps -eo pid,user,comm,args | grep -iE 'xmrig|minerd|kdevtmpfsi|kinsing|sysrv|migrations|cnrig|dbused|tsm|watchbog|tor2web|fakeplugd' | grep -v grep >> $out || echo 'OK - no known miner/botnet names running' >> $out\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: kill -9 <pid> ; check parent: ps -o ppid= -p <pid>' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [2] Diamorphine rootkit (hides via signal 31) --' >> $out\n"
    "ENTER\n"
    "STRING lsmod 2>/dev/null | grep -i diamorphine >> $out && echo 'CRITICAL: Diamorphine rootkit loaded' >> $out || echo 'OK - Diamorphine not in lsmod (but may be hidden)' >> $out\n"
    "ENTER\n"
    "STRING kill -31 0 2>&1 | head -1 >> $out\n"
    "ENTER\n"
    "STRING echo '# Send signal 31 to PID 0: error means clean. No error + new modules visible = rootkit revealing itself' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [3] ld.so.preload hijack (library injection) --' >> $out\n"
    "ENTER\n"
    "STRING if [ -s /etc/ld.so.preload ]; then echo 'WARNING: /etc/ld.so.preload non-empty:' >> $out; cat /etc/ld.so.preload >> $out; else echo 'OK - /etc/ld.so.preload empty or missing' >> $out; fi\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: cat /etc/ld.so.preload should be empty on healthy systems. Investigate listed .so files.' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [4] SSH authorized_keys (unauthorized backdoors) --' >> $out\n"
    "ENTER\n"
    "STRING for u in $(cut -d: -f1,6 /etc/passwd | grep -v nologin); do d=$(echo $u|cut -d: -f2); k=$d/.ssh/authorized_keys; [ -f $k ] && (echo \"User: $(echo $u|cut -d: -f1) ($k):\" >> $out; cat $k >> $out; echo '' >> $out); done\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: review every key; remove unknown ones with sed -i /pattern/d ~/.ssh/authorized_keys' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [5] Cron jobs (system + per-user) --' >> $out\n"
    "ENTER\n"
    "STRING (ls -la /etc/cron* 2>/dev/null; cat /etc/crontab 2>/dev/null; for u in $(cut -f1 -d: /etc/passwd); do crontab -u $u -l 2>/dev/null | grep -v '^#' | grep . && echo \"^^ for user $u\"; done) >> $out\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: crontab -u <user> -e ; remove malicious entries' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [6] Webshells in common web roots (PHP eval/base64) --' >> $out\n"
    "ENTER\n"
    "STRING grep -rEl --include='*.php' '(eval *\\( *(base64_decode|gzinflate|str_rot13)|assert *\\( *\\$_(GET|POST|REQUEST)|preg_replace.*\\/e|passthru *\\( *\\$_)' /var/www /var/html 2>/dev/null >> $out\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: review each match; quarantine with: mv <file> <file>.quarantine' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [7] Recently modified binaries in /tmp /var/tmp /dev/shm --' >> $out\n"
    "ENTER\n"
    "STRING find /tmp /var/tmp /dev/shm -type f -mtime -3 -executable 2>/dev/null | head -30 >> $out\n"
    "ENTER\n"
    "STRING echo '# Legitimate processes rarely place executables in /tmp.  Hash with sha256sum and check VirusTotal.' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [8] Processes with deleted-but-running binaries (in-memory only) --' >> $out\n"
    "ENTER\n"
    "STRING ls -l /proc/*/exe 2>/dev/null | grep deleted >> $out || echo 'OK - no processes with deleted exe links' >> $out\n"
    "ENTER\n"
    "STRING echo '# Common evasion: process unlinks its own binary after launch.  Investigate the deleted ones.' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [9] Suspicious systemd service unit files --' >> $out\n"
    "ENTER\n"
    "STRING grep -rE 'ExecStart=.*(curl|wget|nc|/tmp/|/dev/shm/|base64 -d)' /etc/systemd/system /lib/systemd/system 2>/dev/null >> $out || echo 'OK - no suspicious ExecStart in systemd units' >> $out\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: systemctl disable --now <unit>; rm /etc/systemd/system/<unit>' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [10] High CPU processes (cryptominer signature) --' >> $out\n"
    "ENTER\n"
    "STRING ps -eo pid,user,%cpu,comm --sort=-%cpu | head -10 >> $out\n"
    "ENTER\n"
    "STRING echo '# Any non-system process with >80% CPU for extended periods on idle host = likely miner.' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '== SCAN COMPLETE ==' >> $out\n"
    "ENTER\n"
    "STRING echo 'Review findings.  Run on a periodic schedule (cron) for continuous monitoring.' >> $out\n"
    "ENTER\n"
    "STRING xdg-open $out 2>/dev/null || less $out\n"
    "ENTER\n";

/* ── macOS: Malware Scan ──────────────────────────────────────────────── */

static const char k_mac_malware_scan[] =
    "REM OverL0rk Sentinel - Malware Scan (macOS)\n"
    "REM Detects: Silver Sparrow, OSX/Shlayer, Pegasus IOCs, XCSSET,\n"
    "REM suspicious LaunchAgents/LaunchDaemons, cryptominers.\n"
    "REM Output: ~/sentinel-malware.txt\n"
    "DELAY 1500\n"
    "GUI SPACE\n"
    "DELAY 700\n"
    "STRING terminal\n"
    "DELAY 700\n"
    "ENTER\n"
    "DELAY 2500\n"
    "STRING out=~/sentinel-malware.txt\n"
    "ENTER\n"
    "STRING echo '== OverL0rk Sentinel - macOS Malware Scan ==' > $out\n"
    "ENTER\n"
    "STRING date >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [1] Silver Sparrow IOC (init_verx.plist) --' >> $out\n"
    "ENTER\n"
    "STRING ls -la ~/Library/LaunchAgents/init_verx.plist /Library/LaunchAgents/init_verx.plist 2>/dev/null >> $out || echo 'OK - Silver Sparrow plist not present' >> $out\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: launchctl unload ~/Library/LaunchAgents/init_verx.plist; rm <plist>' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [2] Non-Apple LaunchAgents (user) --' >> $out\n"
    "ENTER\n"
    "STRING ls -la ~/Library/LaunchAgents 2>/dev/null >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [3] Non-Apple LaunchDaemons (system, often persistence) --' >> $out\n"
    "ENTER\n"
    "STRING ls -la /Library/LaunchDaemons 2>/dev/null | grep -vE 'com\\.apple\\.|com\\.microsoft\\.|com\\.google\\.|com\\.adobe\\.' >> $out\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: sudo launchctl unload /Library/LaunchDaemons/<file>.plist ; sudo rm <file>' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [4] OSX/Shlayer / adware (.pkg installers in temp) --' >> $out\n"
    "ENTER\n"
    "STRING find /tmp /private/tmp -name '*.pkg' -mtime -7 2>/dev/null >> $out || echo 'OK - no recent .pkg in /tmp' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [5] Cryptominer process names --' >> $out\n"
    "ENTER\n"
    "STRING ps aux | grep -iE 'xmrig|minerd|cryptonight|monero' | grep -v grep >> $out || echo 'OK - no miner process names' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [6] Persistence via login items --' >> $out\n"
    "ENTER\n"
    "STRING osascript -e 'tell application \"System Events\" to get the name of every login item' 2>/dev/null >> $out\n"
    "ENTER\n"
    "STRING echo '# REMEDIATION: System Settings > General > Login Items, remove unrecognized entries' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [7] Recently modified binaries in /tmp /var/tmp --' >> $out\n"
    "ENTER\n"
    "STRING find /tmp /var/tmp /private/tmp -type f -mtime -3 -perm +111 2>/dev/null | head -30 >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [8] Suspicious cron / launchd unsigned plists --' >> $out\n"
    "ENTER\n"
    "STRING for p in /Library/LaunchDaemons/*.plist /Library/LaunchAgents/*.plist; do codesign -dv \"$p\" 2>&1 | grep -E 'not signed|invalid' && echo \"^ $p\" >> $out; done >> $out 2>&1\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [9] Periodic / at jobs --' >> $out\n"
    "ENTER\n"
    "STRING crontab -l 2>/dev/null >> $out; atq 2>/dev/null >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '-- [10] Pegasus indicators (advanced; partial) --' >> $out\n"
    "ENTER\n"
    "STRING ls /private/var/db/com.apple.xpc.launchd/disabled.501.plist 2>/dev/null >> $out\n"
    "ENTER\n"
    "STRING for d in bh.plist com.apple.WebKit.Networking.xpc; do find / -name $d 2>/dev/null | head -3 >> $out; done\n"
    "ENTER\n"
    "STRING echo '# Pegasus is sophisticated state-grade malware.  If suspicion is real, contact a forensics professional + use mvt-ios.' >> $out\n"
    "ENTER\n"
    "STRING echo '' >> $out; echo '== SCAN COMPLETE ==' >> $out\n"
    "ENTER\n"
    "STRING open -a TextEdit $out\n"
    "ENTER\n";

/* ── Registry ────────────────────────────────────────────────────────────── */

/*
 * The order here defines the menu order.  Group by OS for easy scrolling.
 * label is displayed in the Submenu (≤ 22 chars looks best on the screen);
 * description is shown on the detail Widget (3 lines × ~16 chars each).
 */
static const SentinelPayload k_payloads[] = {
    /* --- Windows --- */
    {
        .id          = "win_portscan",
        .label       = "Win: Port Scan",
        .description = "PowerShell:\nLISTEN+EST TCP/UDP\n-> C:\\Temp\\ports.txt",
        .ducky       = k_win_portscan,
    },
    {
        .id          = "win_avcheck",
        .label       = "Win: AV Check",
        .description = "Defender status,\nthreats, 3rd-party AV\n-> C:\\Temp\\av.txt",
        .ducky       = k_win_avcheck,
    },
    {
        .id          = "win_procaudit",
        .label       = "Win: Process Audit",
        .description = "Top procs + unsigned\nbinaries + net sockets\n-> C:\\Temp\\proc.txt",
        .ducky       = k_win_procaudit,
    },
    {
        .id          = "win_netaudit",
        .label       = "Win: Net Audit",
        .description = "Firewall, ARP, DNS,\nWi-Fi profiles\n-> C:\\Temp\\net.txt",
        .ducky       = k_win_netaudit,
    },
    {
        .id          = "win_hashcheck",
        .label       = "Win: Hash Check",
        .description = "SHA-256 of cmd.exe,\nexplorer, lsass, ...\n-> C:\\Temp\\hash.txt",
        .ducky       = k_win_hashcheck,
    },
    /* --- Windows malware detection --- */
    {
        .id          = "win_malware_scan",
        .label       = "Win: Malware Scan",
        .description = "Top 10 IOCs: Mimikatz\nCobaltStr, miners, RAT\n-> sentinel-malware",
        .ducky       = k_win_malware_scan,
    },
    {
        .id          = "win_ransomware",
        .label       = "Win: Ransomware",
        .description = "Encrypted ext + notes\n+ shadow copies check\n-> sentinel-ransom",
        .ducky       = k_win_ransomware,
    },
    /* --- Linux --- */
    {
        .id          = "linux_portaudit",
        .label       = "Linux: Port Audit",
        .description = "ss + lsof + iptables\n(Ctrl+Alt+T terminal)\n-> ~/sentinel-ports",
        .ducky       = k_linux_portaudit,
    },
    {
        .id          = "linux_procaudit",
        .label       = "Linux: Proc Audit",
        .description = "ps tree, top CPU/RAM\nsystemd failed units\n-> ~/sentinel-proc",
        .ducky       = k_linux_procaudit,
    },
    {
        .id          = "linux_malware_scan",
        .label       = "Linux: Malware Scan",
        .description = "Miners, rootkits, web-\nshells, SSH backdoors\n-> ~/sentinel-malware",
        .ducky       = k_linux_malware_scan,
    },
    /* --- macOS --- */
    {
        .id          = "mac_portaudit",
        .label       = "Mac: Port Audit",
        .description = "lsof LISTEN+EST, pf,\nLaunchDaemons (Spot.)\n-> ~/sentinel-ports",
        .ducky       = k_mac_portaudit,
    },
    {
        .id          = "mac_malware_scan",
        .label       = "Mac: Malware Scan",
        .description = "Silver Sparrow, Shlayer\nPegasus, LaunchDaemons\n-> ~/sentinel-malware",
        .ducky       = k_mac_malware_scan,
    },
};

const SentinelPayload* sentinel_payloads_get(size_t* out_count) {
    if(out_count) *out_count = sizeof(k_payloads) / sizeof(k_payloads[0]);
    return k_payloads;
}
