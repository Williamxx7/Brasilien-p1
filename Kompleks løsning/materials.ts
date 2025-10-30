import express from "express";
import pool from "./db";

const router = express.Router();

// GET all materials
router.get("/", async (_, res) => {
  try {
    const result = await pool.query('SELECT * FROM public."Materials" ORDER BY "Material_id" ASC');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: "Internal Server Error" });
  }
});

// GET one material by ID
router.get("/:id", async (req, res) => {
  try {
    const { id } = req.params;
    const result = await pool.query('SELECT * FROM public."Materials" WHERE "Material_id" = $1', [id]);
    if (result.rows.length === 0) return res.status(404).json({ error: "Material not found" });
    res.json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: "Internal Server Error" });
  }
});

// POST - add new material
router.post("/", async (req, res) => {
  try {
    const { Material_name, Material_group } = req.body;
    if (!Material_name) return res.status(400).json({ error: "Material_name required" });

    const result = await pool.query(
      'INSERT INTO public."Materials" ("Material_name", "Material_group") VALUES ($1, $2) RETURNING *',
      [Material_name, Material_group || null]
    );

    res.status(201).json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: "Internal Server Error" });
  }
});

export default router;
