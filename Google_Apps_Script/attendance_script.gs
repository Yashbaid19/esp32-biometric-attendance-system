function doGet(e) {

  var ss = SpreadsheetApp.getActiveSpreadsheet();

  var logSheet = ss.getSheetByName("Logs");

  var reportSheet = ss.getSheetByName("Attendance Report");

  var employeeSheet = ss.getSheetByName("Employees");

  var id = e.parameter.id;

  var name = e.parameter.name;

  var email = e.parameter.email;

  var currentTime = new Date();

  var rows = logSheet.getDataRange().getValues();

  var action = "IN";

  var totalDuration = "";

  for (var i = rows.length - 1; i >= 1; i--) {

    if (rows[i][2].toString() == id.toString()) {

      var lastAction = rows[i][3];

      var lastTime = new Date(rows[i][0]);

      if (lastAction == "IN") {

        action = "OUT";

        var diffMs = currentTime - lastTime;

        var diffMins = Math.floor(diffMs / 60000);

        totalDuration = diffMins + " mins";
      }

      break;
    }
  }

  logSheet.appendRow([
    currentTime,
    name,
    id,
    action,
    totalDuration
  ]);

  var today = Utilities.formatDate(
    currentTime,
    Session.getScriptTimeZone(),
    "yyyy-MM-dd"
  );

  var reportData = reportSheet.getDataRange().getValues();

  var alreadyExists = false;

  for (var j = 1; j < reportData.length; j++) {

    var sheetDate = Utilities.formatDate(
      new Date(reportData[j][0]),
      Session.getScriptTimeZone(),
      "yyyy-MM-dd"
    );

    if (
      sheetDate == today &&
      reportData[j][2].toString() == id.toString()
    ) {
      alreadyExists = true;
      break;
    }
  }

  if (!alreadyExists) {

    reportSheet.appendRow([
      today,
      name,
      id,
      "Present"
    ]);
  }

  var employeeData = employeeSheet.getDataRange().getValues();

  var employeeExists = false;

  for (var k = 1; k < employeeData.length; k++) {

    if (employeeData[k][0].toString() == id.toString()) {
      employeeExists = true;
      break;
    }
  }

  if (!employeeExists) {

    employeeSheet.appendRow([
      id,
      name,
      email,
      ""
    ]);
  }

  if (
    email &&
    email != "" &&
    email.includes("@")
  ) {

    GmailApp.sendEmail(
      email,
      "Attendance Update",
      "Hello " + name +
      ",\n\nYour attendance has been marked as " +
      action +
      ".\nTime: " + currentTime +
      "\n\nRegards,\nBiometric Attendance System"
    );
  }

  updateDashboardStats();
  
  return ContentService.createTextOutput(action);
}

function markAbsentees() {

  var ss = SpreadsheetApp.getActiveSpreadsheet();

  var employeeSheet = ss.getSheetByName("Employees");

  var reportSheet = ss.getSheetByName("Attendance Report");

  var employees = employeeSheet.getDataRange().getValues();

  var reports = reportSheet.getDataRange().getValues();

  var today = Utilities.formatDate(
    new Date(),
    Session.getScriptTimeZone(),
    "yyyy-MM-dd"
  );

  for (var i = 1; i < employees.length; i++) {

    var empID = employees[i][0];

    var empName = employees[i][1];

    var empEmail = employees[i][2];

    var found = false;

    for (var j = 1; j < reports.length; j++) {

      var sheetDate = Utilities.formatDate(
        new Date(reports[j][0]),
        Session.getScriptTimeZone(),
        "yyyy-MM-dd"
      );

      if (
        sheetDate == today &&
        reports[j][2].toString() == empID.toString()
      ) {
        found = true;
        break;
      }
    }

    if (!found) {

      reportSheet.appendRow([
        today,
        empName,
        empID,
        "Absent"
      ]);

      if (
        empEmail &&
        empEmail != "" &&
        empEmail.includes("@")
      ) {

        GmailApp.sendEmail(
          empEmail,
          "Absent Notification",
          "Hello " + empName +
          ",\n\nYou were marked ABSENT today (" +
          today +
          ").\n\nRegards,\nBiometric Attendance System"
        );
      }
    }
  }
}
function syncEmployeesFromFirebase() {

  var firebaseURL = "https://biometricattendance-ae474-default-rtdb.asia-southeast1.firebasedatabase.app/Employees.json";

  var response = UrlFetchApp.fetch(firebaseURL);

  var data = JSON.parse(response.getContentText());

  var ss = SpreadsheetApp.getActiveSpreadsheet();

  var employeeSheet = ss.getSheetByName("Employees");

  var existingData = employeeSheet.getDataRange().getValues();

  var existingIDs = [];

  for (var i = 1; i < existingData.length; i++) {

    existingIDs.push(existingData[i][0].toString());
  }

  for (var id in data) {

    if (data[id] == null) {
      continue;
    }

    if (
      data[id].id == null ||
      data[id].name == null ||
      data[id].email == null
    ) {
      continue;
    }

    if (!existingIDs.includes(id.toString())) {

      employeeSheet.appendRow([
        data[id].id,
        data[id].name,
        data[id].email,
        data[id].template || ""
      ]);
    }
  }
}
function updateBlynkDashboard() {

  var BLYNK_TOKEN = "Ip5BmQJUzPZJF1ye5Q_FRfx-QUxjlOey";

  var ss = SpreadsheetApp.getActiveSpreadsheet();

  var reportSheet = ss.getSheetByName("Attendance Report");

  var employeeSheet = ss.getSheetByName("Employees");

  var logSheet = ss.getSheetByName("Logs");

  var today = Utilities.formatDate(
    new Date(),
    Session.getScriptTimeZone(),
    "yyyy-MM-dd"
  );

  var reportData = reportSheet.getDataRange().getValues();

  var employeeData = employeeSheet.getDataRange().getValues();

  var logData = logSheet.getDataRange().getValues();

  var presentCount = 0;

  var absentCount = 0;

  var insideCount = 0;

  for (var i = 1; i < reportData.length; i++) {

    var sheetDate = Utilities.formatDate(
      new Date(reportData[i][0]),
      Session.getScriptTimeZone(),
      "yyyy-MM-dd"
    );

    if (sheetDate == today) {

      if (reportData[i][3] == "Present") {
        presentCount++;
      }

      if (reportData[i][3] == "Absent") {
        absentCount++;
      }
    }
  }

  var totalEmployees = employeeData.length - 1;

  var latestStatus = {};

  for (var j = 1; j < logData.length; j++) {

    latestStatus[logData[j][2]] = logData[j][3];
  }

  for (var id in latestStatus) {

    if (latestStatus[id] == "IN") {
      insideCount++;
    }
  }

  var lastEvent = "";

  if (logData.length > 1) {

    var lastRow = logData[logData.length - 1];

    lastEvent = lastRow[1] + " " + lastRow[3];
  }

  var urls = [

    "https://blynk.cloud/external/api/update?token=" +
    BLYNK_TOKEN + "&V10=" + presentCount,

    "https://blynk.cloud/external/api/update?token=" +
    BLYNK_TOKEN + "&V11=" + absentCount,

    "https://blynk.cloud/external/api/update?token=" +
    BLYNK_TOKEN + "&V12=" + totalEmployees,

    "https://blynk.cloud/external/api/update?token=" +
    BLYNK_TOKEN + "&V13=" + insideCount,

    "https://blynk.cloud/external/api/update?token=" +
    BLYNK_TOKEN + "&V14=" +
    encodeURIComponent(lastEvent)
  ];

  for (var k = 0; k < urls.length; k++) {

    UrlFetchApp.fetch(urls[k]);
  }
}
function updateDashboardStats() {

  var ss =
  SpreadsheetApp.getActiveSpreadsheet();

  var employeeSheet =
  ss.getSheetByName("Employees");

  var reportSheet =
  ss.getSheetByName("Attendance Report");

  var totalEmployees =
  employeeSheet.getLastRow() - 1;

  var reportData =
  reportSheet.getDataRange().getValues();

  var today =
  Utilities.formatDate(
    new Date(),
    Session.getScriptTimeZone(),
    "yyyy-MM-dd"
  );

  var presentToday = 0;

  for (var i = 1; i < reportData.length; i++) {

    if (
      reportData[i][0] == today &&
      reportData[i][3] == "Present"
    ) {
      presentToday++;
    }
  }

  var absentToday =
  totalEmployees - presentToday;

  var firebaseURL =
  "https://biometricattendance-ae474-default-rtdb.asia-southeast1.firebasedatabase.app/DashboardStats.json";

  var payload = {
    presentToday: presentToday,
    absentToday: absentToday,
    totalEmployees: totalEmployees
  };

  var options = {
    method: "put",
    contentType: "application/json",
    payload: JSON.stringify(payload)
  };

  UrlFetchApp.fetch(firebaseURL, options);
}