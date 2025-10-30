
import express from "express";
//importere databasen fra db.ts filen
import pool from "./db";
import dotenv from "dotenv";
dotenv.config();

//opretter en express app
const app = express();
const port = process.env.PORT || 4000;

// laver en "route": get /material
app.get("/materials", async (req, res) => {
  try {
    // laver en sql forespørgelse
    const result = await pool.query('SELECT * FROM public."Materials" ORDER BY "Material_id" ASC');
    //sender data som json
    res.json(result.rows);
    //evt fejl
  } catch (err) {
    console.error("Database error:", err);
    res.status(500).json({ error: "Internal Server Error" });
  }
});

//starter serveren
app.listen(port, () => {
  console.log(`✅ Server running on http://localhost:${port}`);
});
