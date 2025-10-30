import express from "express";
import pool from "./db";

const router = express.Router();

// GET all devices
router.get("/", async (_, res) => {
  try {
    const result = await pool.query('SELECT * FROM public."Devices" ORDER BY device_id ASC');
    res.json(result.rows);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: "Internal Server Error" });
  }
});

// GET single device by ID
router.get("/:id", async (req, res) => {
  try {
    const { id } = req.params;
    const result = await pool.query('SELECT * FROM public."Devices" WHERE device_id = $1', [id]);
    if (result.rows.length === 0) return res.status(404).json({ error: "Device not found" });
    res.json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: "Internal Server Error" });
  }
});

export default router;
