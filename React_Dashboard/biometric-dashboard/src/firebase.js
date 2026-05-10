import { initializeApp } from "firebase/app";

import { getDatabase } from "firebase/database";

const firebaseConfig = {
  apiKey: "AIzaSyAZ-Ls6B0yiZ3tL7G6_YR8oZi6H4cVgpTI",
  authDomain: "biometricattendance-ae474.firebaseapp.com",
  databaseURL: "https://biometricattendance-ae474-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "biometricattendance-ae474",
  storageBucket: "biometricattendance-ae474.firebasestorage.app",
  messagingSenderId: "507585742596",
  appId: "1:507585742596:web:09842bfbdb7ac56d88f251",
  measurementId: "G-D70E3TE75C"
};

const app = initializeApp(firebaseConfig);

export const db = getDatabase(app);