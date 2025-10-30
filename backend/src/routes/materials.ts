import express from "express";
import pool from "../db";

const router = express.Router();

// Hent alle materialer
router.get("/", async (_, res) => {
  const result = await pool.query('SELECT * FROM public."Materials" ORDER BY "Material_id" ASC');
  res.json(result.rows);
});

// Hent ét materiale
router.get("/:id", async (req, res) => {
  const { id } = req.params;
  const result = await pool.query('SELECT * FROM public."Materials" WHERE "Material_id"=$1', [id]);
  if (result.rows.length === 0) return res.status(404).json({ error: "Not found" });
  res.json(result.rows[0]);
});

export default router;
