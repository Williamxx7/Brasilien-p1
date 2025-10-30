import express from "express";
import cors from "cors";
import dotenv from "dotenv";
import deviceRoutes from "./devices";
import materialRoutes from "./materials";


dotenv.config();
const app = express();
const port = process.env.PORT || 4000;

app.use(cors());
app.use(express.json());

// routes
app.use("/devices", deviceRoutes);
app.use("/materials", materialRoutes);


// health check
app.get("/", (_, res) => res.send("✅ API is running"));

app.listen(port, () => console.log(`Server running on http://localhost:${port}`));
