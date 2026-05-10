import { useEffect, useState } from "react";

import {
  ref,
  onValue
} from "firebase/database";

import { db } from "./firebase";

function App() {

  const [employees, setEmployees] =
  useState([]);

  const [stats, setStats] =
  useState({
    presentToday: 0,
    absentToday: 0,
    totalEmployees: 0
  });

  useEffect(() => {

    const employeesRef =
    ref(db, "Employees");

    onValue(employeesRef, (snapshot) => {

      const data = snapshot.val();

      if (data) {

        const employeeList =
        Object.values(data);

        setEmployees(employeeList);
      }
    });

  }, []);

  useEffect(() => {

    const statsRef =
    ref(db, "DashboardStats");

    onValue(statsRef, (snapshot) => {

      const data = snapshot.val();

      if (data) {

        setStats(data);
      }
    });

  }, []);

  return (
    <div className="min-h-screen bg-slate-900 text-white p-6">

      <h1 className="text-4xl font-bold mb-8">
        Biometric Attendance Dashboard
      </h1>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">

        <div className="bg-slate-800 p-6 rounded-2xl shadow-lg">
          <h2 className="text-xl font-semibold">
            Present Today
          </h2>

          <p className="text-5xl mt-4 font-bold text-green-400">
            {stats.presentToday}
          </p>
        </div>

        <div className="bg-slate-800 p-6 rounded-2xl shadow-lg">
          <h2 className="text-xl font-semibold">
            Absent Today
          </h2>

          <p className="text-5xl mt-4 font-bold text-red-400">
            {stats.absentToday}
          </p>
        </div>

        <div className="bg-slate-800 p-6 rounded-2xl shadow-lg">
          <h2 className="text-xl font-semibold">
            Total Employees
          </h2>

          <p className="text-5xl mt-4 font-bold text-blue-400">
            {stats.totalEmployees}
          </p>
        </div>

      </div>

      <div className="mt-10 bg-slate-800 p-6 rounded-2xl shadow-lg">

        <h2 className="text-2xl font-bold mb-4">
          Employees
        </h2>

        <div className="space-y-3">

          {employees.map((employee, index) => (

            <div
              key={index}
              className="bg-slate-700 p-4 rounded-xl"
            >
              {employee.name}
            </div>

          ))}

        </div>

      </div>
      <div className="mt-10 bg-slate-800 p-6 rounded-2xl shadow-lg">
        <h2 className="text-2xl font-bold mb-4">
          Attendance Sheet
        </h2>

        <iframe
          src="https://docs.google.com/spreadsheets/d/e/2PACX-1vRvrkqicwwPrpYwh8hgtPG4VhPNsfvFzdrhVQ1BTICPDmy-wVzoHQ3YFGHXPbmO6kCxkBXnMxjVH9Ab/pubhtml?gid=0&amp;single=true&amp;widget=true&amp;headers=false"          width="100%"
          height="500"
          className="rounded-xl bg-white"
        ></iframe>

      </div>

    </div>
  );
}

export default App;