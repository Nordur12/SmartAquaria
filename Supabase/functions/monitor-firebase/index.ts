import { serve } from "https://deno.land/std@0.203.0/http/server.ts";
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.49.1";
import { getDatabase } from "npm:firebase-admin@11.10.1/database";
import { getFirestore } from "npm:firebase-admin@11.10.1/firestore";
import { initializeApp, cert } from "npm:firebase-admin@11.10.1/app";
import { ref, get } from "https://www.gstatic.com/firebasejs/9.15.0/firebase-database.js";

// ✅ Firebase Realtime Database URL
const DATABASE_URL = "https://smartaquaria-9ad3b-default-rtdb.asia-southeast1.firebasedatabase.app";

// ✅ Supabase Setup
const SUPABASE_URL = Deno.env.get("SUPABASE_URL")!;
const SUPABASE_SERVICE_ROLE_KEY = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
const _supabase = createClient(SUPABASE_URL, SUPABASE_SERVICE_ROLE_KEY);

// ✅ Firebase Setup
const SERVICE_ACCOUNT_JSON = Deno.env.get("FIREBASE_SERVICE_ACCOUNT")!;
const SERVICE_ACCOUNT = JSON.parse(SERVICE_ACCOUNT_JSON);

const firebaseApp = initializeApp({
  credential: cert(SERVICE_ACCOUNT),
  databaseURL: DATABASE_URL,
});
const db = getDatabase(firebaseApp);
const firestore = getFirestore();

type DeviceData = {
  aquariumId: string;
  userId: string;
  ph?: { pHLevel?: number };
  turbidity?: { condition?: string };
  temperature?: { temperature?: number };
};


// Store last known data to prevent duplicate alerts
const lastKnownData: Record<string, { pHLevel?: number; NTUCondition?: string; temperature?: number }> = {};

async function pollFirebase() {
  try {
    const devicesSnapshot = await getDatabase(firebaseApp).ref("devices").get();
    const devices = devicesSnapshot.val() as Record<string, DeviceData>;


    if (!devices) return;

    for (const [deviceId, deviceData] of Object.entries(devices)) {
      const { aquariumId, userId, ph, turbidity, temperature } = deviceData;

      if (!userId || !aquariumId) continue;

      const pHLevel = ph?.pHLevel;
      const NTUCondition = turbidity?.condition;
      const temp = temperature?.temperature;

      // Fetch aquarium name from Firestore
      const aquariumDoc = await firestore.collection("aquariums").doc(aquariumId).get();
      if (!aquariumDoc.exists) {
        console.error(`❌ Aquarium not found (ID: ${aquariumId})`);
        continue;
      }
      const aquariumName = aquariumDoc.data()?.name || "Unknown Aquarium";

      // Ensure lastKnownData exists for this device
      lastKnownData[deviceId] = lastKnownData[deviceId] || {};

      // Alerts logic
      if (pHLevel !== undefined && pHLevel > 8.5 && lastKnownData[deviceId].pHLevel !== pHLevel) {
        await sendNotification(userId, deviceId, aquariumName, { pHLevel });
      }
      if (pHLevel !== undefined && pHLevel < 6.0 && lastKnownData[deviceId].pHLevel !== pHLevel) {
        await sendNotification(userId, deviceId, aquariumName, { pHLevel });
      }
      if (NTUCondition && NTUCondition !== "CLEAR" && lastKnownData[deviceId].NTUCondition !== NTUCondition) {
        await sendNotification(userId, deviceId, aquariumName, { NTUCondition });
      }
      if (temp !== undefined && temp > 28 && lastKnownData[deviceId].temperature !== temp) {
        await sendNotification(userId, deviceId, aquariumName, { temperature: temp });
      }
      if (temp !== undefined && temp < 23 && lastKnownData[deviceId].temperature !== temp) {
        await sendNotification(userId, deviceId, aquariumName, { temperature: temp });
      }

      // Update last known values
      lastKnownData[deviceId] = { pHLevel, NTUCondition, temperature: temp };
    }
  } catch (error) {
    console.error("❌ Error polling Firebase:", error);
  }
}

async function sendNotification(
  userId: string,
  deviceId: string,
  aquariumName: string,
  data: Record<string, any>
) {
  console.log(`⚠️ Alert detected | User: ${userId}, Device: ${deviceId}, Data:`, data);

  try {
    const response = await fetch("https://lhdhuonairkdxzpaaqpi.supabase.co/functions/v1/notification", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Authorization": `Bearer ${SUPABASE_SERVICE_ROLE_KEY}`,
      },
      body: JSON.stringify({ userId, deviceId, aquariumName, ...data }),
    });

    if (!response.ok) {
      console.error("❌ Failed to send notification:", await response.text());
    }
  } catch (error) {
    console.error("❌ Error sending notification:", error);
  }
}

// Poll Firebase every 10 seconds
setInterval(pollFirebase, 10000);

// Start HTTP server
serve(() => new Response("Polling active 🚀", { status: 200 }));

