const I18N = {
  en: {
    title: "NP05J Boot Helper",
    deviceLabel: "Device", slotLabel: "Slot", modeLabel: "Boot Mode", touchLabel: "Touch IC",
    tabBoot: "Boot", tabStorage: "Install / Storage", tabMaint: "Maintenance",
    bootTitle: "One-Tap OS & BIOS Switcher",
    bootDesc: "Writes a one-shot NEXTv1 request into the SPLITv1 combined boot image and reboots directly.",
    btnWin11: "Reboot to Windows 11 ARM",
    btnLinux: "Reboot to Linux (DTB/ACPI)",
    btnUefiMenu: "Reboot to Touch Boot Menu",
    btnBios: "Reboot to Unlocked BIOS Setup",
    storageTitle: "Combined Boot Selector & Stock Restore",
    storageDesc: "Backup stock Android boot v4, install pre-UEFI touch BootSelect + Mu-Silicium, or restore byte-for-byte stock.",
    timeoutLabel: "Pre-UEFI Selector Timeout (sec):",
    btnInspect: "Inspect Active Boot Slot",
    btnInstall: "Backup Stock + Install Combined SunUEFI Boot",
    btnRestore: "Restore Verified Stock Android Boot (RSTRv1)",
    maintTitle: "Hardware Reset Modes & Diagnostics",
    maintDesc: "PMK8550 SDAM reboot targets, Qualcomm EDL 9008, and live helper output.",
    btnClearReq: "Clear Pending NEXTv1 Request",
    btnRecovery: "Reboot Recovery",
    btnBootloader: "Reboot Bootloader / Fastboot",
    btnEdl: "Reboot Qualcomm EDL (9008)",
    consoleLabel: "Output Log"
  },
  th: {
    title: "ตัวช่วยบูต SunUEFI (NP05J)",
    deviceLabel: "อุปกรณ์", slotLabel: "สล็อต", modeLabel: "สถานะบูต", touchLabel: "ทัชสกรีน",
    tabBoot: "สลับระบบบูต", tabStorage: "ติดตั้ง / กู้คืน", tabMaint: "ซ่อมบำรุง",
    bootTitle: "สลับเข้า Windows 11 / Linux / BIOS ได้ในคลิกเดียว",
    bootDesc: "เขียนคำสั่ง NEXTv1 ลงพาร์ติชัน boot ที่รวม BootSelect+UEFI แล้วรีบูตเข้าปลายทางทันที",
    btnWin11: "รีบูตเข้า Windows 11 ARM",
    btnLinux: "รีบูตเข้า Linux (DTB/ACPI)",
    btnUefiMenu: "รีบูตเข้าหน้าจอ UEFI Boot Menu (รองรับสัมผัส)",
    btnBios: "รีบูตเข้าหน้าจอ BIOS Setup (ปลดล็อคทุกหัวข้อ)",
    storageTitle: "จัดการอิมเมจ BootSelector & กู้คืน Stock Boot",
    storageDesc: "สำรอง boot เดิม (v4), ติดตั้ง pre-UEFI BootSelect ที่รองรับทัชสกรีน, หรือกู้คืนอิมเมจเดิม 100%",
    timeoutLabel: "เวลานับถอยหลังหน้าเลือกบูต (วินาที):",
    btnInspect: "ตรวจสอบพาร์ติชัน Boot ปัจจุบัน",
    btnInstall: "สำรอง Stock + ติดตั้ง Combined SunUEFI Boot",
    btnRestore: "กู้คืน Stock Android Boot เดิม (RSTRv1)",
    maintTitle: "โหมดรีเซ็ตฮาร์ดแวร์และการตรวจสอบ",
    maintDesc: "รีบูตเข้า Recovery / Fastboot / Qualcomm EDL 9008 และล้างค่า NEXTv1",
    btnClearReq: "ล้างคำสั่ง NEXTv1 ที่ค้างอยู่",
    btnRecovery: "รีบูตเข้า Recovery",
    btnBootloader: "รีบูตเข้า Bootloader / Fastboot",
    btnEdl: "เข้าโหมด Qualcomm EDL (9008)",
    consoleLabel: "บันทึกการทำงาน"
  },
  zh: {
    title: "NP05J SunUEFI 启动助手",
    deviceLabel: "设备", slotLabel: "槽位", modeLabel: "引导状态", touchLabel: "触控芯片",
    tabBoot: "系统切换", tabStorage: "安装与还原", tabMaint: "硬件维护",
    bootTitle: "一键重启切换系统与 BIOS",
    bootDesc: "写入一次性 NEXTv1 引导请求到 SPLITv1 复合分区后自动重启。",
    btnWin11: "重启进入 Windows 11 ARM",
    btnLinux: "重启进入 Linux (DTB/ACPI)",
    btnUefiMenu: "重启进入触控 UEFI 启动菜单",
    btnBios: "重启进入全解锁 BIOS 设置",
    storageTitle: "双系统引导安装与原厂还原",
    storageDesc: "自动备份原厂 boot v4，打包触控 BootSelect 与 Mu-Silicium UEFI，支持无损字节级还原。",
    timeoutLabel: "启动选择器等待时间 (秒):",
    btnInspect: "检查当前 Boot 槽位",
    btnInstall: "备份原厂并安装 SunUEFI 复合引导",
    btnRestore: "无损还原原厂 Android Boot (RSTRv1)",
    maintTitle: "硬件重启模式与诊断",
    maintDesc: "PMK8550 重启原因、高通 9008 下载模式与状态日志。",
    btnClearReq: "清除待执行的 NEXTv1 请求",
    btnRecovery: "重启到 Recovery",
    btnBootloader: "重启到 Bootloader / Fastboot",
    btnEdl: "重启到 Qualcomm EDL (9008)",
    consoleLabel: "运行日志"
  }
};

const MOD_HELPER = "/data/adb/modules/np05j_sunuefi_helper/common/np05j-helper.sh";
const logEl = document.getElementById("logOut");

function appendLog(msg) {
  logEl.textContent = (logEl.textContent === "Ready." ? "" : logEl.textContent + "\n") + msg;
  logEl.scrollTop = logEl.scrollHeight;
}

function setLang(lang) {
  const dict = I18N[lang] || I18N.en;
  document.querySelectorAll("[data-i18n]").forEach((el) => {
    const k = el.getAttribute("data-i18n");
    if (dict[k]) el.textContent = dict[k];
  });
  document.querySelectorAll("[data-lang]").forEach((b) => {
    b.classList.toggle("active", b.getAttribute("data-lang") === lang);
  });
}

function execShell(cmd) {
  return new Promise((resolve) => {
    if (window.ksu && typeof window.ksu.exec === "function") {
      const cb = "_ksu_cb_" + Math.random().toString(36).slice(2);
      window[cb] = (code, stdout, stderr) => {
        delete window[cb];
        resolve({ code, stdout: stdout || "", stderr: stderr || "" });
      };
      window.ksu.exec(cmd, "{}", cb);
      return;
    }
    resolve({
      code: 0,
      stdout: "[Preview Mode] Would run: " + cmd + "\nslot=_a\nsunuefi_combined=0 (preview)\n",
      stderr: ""
    });
  });
}

async function runHelper(args) {
  appendLog("$ np05j-helper.sh " + args);
  const res = await execShell(`sh ${MOD_HELPER} ${args}`);
  if (res.stdout) appendLog(res.stdout.trim());
  if (res.stderr) appendLog("[stderr] " + res.stderr.trim());
  if (args.startsWith("status")) {
    for (const line of res.stdout.split("\n")) {
      if (line.startsWith("slot=")) document.getElementById("stSlot").textContent = line.slice(5);
      if (line.startsWith("device=")) document.getElementById("stDevice").textContent = line.slice(7);
      if (line.startsWith("sunuefi_combined=")) {
        document.getElementById("stMode").textContent = line.includes("1") ? "SunUEFI SPLITv1" : "Stock / Raw";
      }
    }
  }
}

document.querySelectorAll("[data-lang]").forEach((btn) => {
  btn.addEventListener("click", () => setLang(btn.getAttribute("data-lang")));
});

document.querySelectorAll(".tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    const name = tab.getAttribute("data-tab");
    document.querySelectorAll(".tab").forEach((t) => t.classList.toggle("active", t === tab));
    document.querySelectorAll(".panel").forEach((p) => p.classList.toggle("active", p.id === `tab-${name}`));
  });
});

document.querySelectorAll("[data-reboot]").forEach((btn) => {
  btn.addEventListener("click", () => runHelper(`reboot ${btn.getAttribute("data-reboot")}`));
});
document.querySelectorAll("[data-reboot-raw]").forEach((btn) => {
  btn.addEventListener("click", () => runHelper(`reboot ${btn.getAttribute("data-reboot-raw")}`));
});

document.getElementById("btnRefresh").addEventListener("click", () => runHelper("status"));
document.getElementById("btnInstall").addEventListener("click", () => {
  const sec = Number(document.getElementById("selTimeout").value || 3);
  runHelper(`install ${sec}`);
});
document.getElementById("btnRestore").addEventListener("click", () => runHelper("restore"));
document.getElementById("btnClearReq").addEventListener("click", () => runHelper("clear-oneshot"));

setLang("en");
runHelper("status");
