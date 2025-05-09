import { serve } from "https://deno.land/std@0.203.0/http/server.ts";
import { getFirestore } from "npm:firebase-admin@11.10.1/firestore";
import { getMessaging } from "npm:firebase-admin@11.10.1/messaging";
import { initializeApp, cert } from "npm:firebase-admin@11.10.1/app";
import { Timestamp } from "npm:firebase-admin@11.10.1/firestore";

// ✅ Initialize Firebase Firestore and Messaging
const SERVICE_ACCOUNT_JSON = Deno.env.get("FIREBASE_SERVICE_ACCOUNT")!;
const SERVICE_ACCOUNT = JSON.parse(SERVICE_ACCOUNT_JSON);
const firebaseApp = initializeApp({ credential: cert(SERVICE_ACCOUNT) });
const firestore = getFirestore(firebaseApp);
const messaging = getMessaging(firebaseApp);

// Function to send FCM notification
async function sendNotification(deviceToken: string, title: string, message: string) {
  try {
    const payload = {
      token: deviceToken,
      notification: { // 🔔 Push notification for background & quit state
        title,
        body: message,
      },
      data: { // 🔥 Data payload for foreground alerts
        title,
        message,
        popup: "true",
      },
      android: {
        priority: "high" as "high",
        notification: {
          channelId: "default",
        },
      },
    };

    const response = await messaging.send(payload);
    console.log(` Notification sent: ${title} - ${message}`);
    return { success: true };
  } catch (error) {
    console.error("❌ Notification error:", error);
    return { success: false, error };
  }
}



// HTTP Server to Handle Sensor Alerts
serve(async (req) => {
  try {
    const { userId, deviceId, aquariumName, pHLevel, NTUCondition, temperature } = await req.json();
    if (!userId || !aquariumName) {
      return new Response(JSON.stringify({ error: "Missing userId or aquariumName" }), { status: 400 });
    }

    // Fetch User's FCM Token
    const userDoc = await firestore.collection("users").doc(userId).get();
    if (!userDoc.exists) {
      return new Response(JSON.stringify({ error: "User not found" }), { status: 404 });
    }
    const userData = userDoc.data();
    const deviceToken = userData?.fcmToken;
    if (!deviceToken) {
      return new Response(JSON.stringify({ error: "No FCM token for user" }), { status: 400 });
    }

    const notifications = [];

    // pH Level Alerts
    if (pHLevel !== undefined && pHLevel > 8.5) {
      notifications.push({ 
        title: "⚠️ High pH Alert!", 
        message: `Your aquarium '${aquariumName}' has a high pH level (${pHLevel})!` });
    }
    if (pHLevel !== undefined && pHLevel < 6.0) {
      notifications.push({ 
        title: "⚠️ Low pH Alert!", 
        message: `Your aquarium '${aquariumName}' has a low pH level (${pHLevel})!` });
    }

    // Turbidity Alerts
    if (NTUCondition && NTUCondition !== "CLEAR") {
      notifications.push({ 
        title: "⚠️ Turbidity Alert!", 
        message: `Your aquarium '${aquariumName}' has ${NTUCondition} water condition!` });
    }

    // Temperature Alerts
    if (temperature !== undefined && temperature > 28) {
      notifications.push({ 
        title: "⚠️ High Temperature Alert!", 
        message: `Your aquarium '${aquariumName}' temperature is too high (${temperature}°C)!` });
    }
    if (temperature !== undefined && temperature < 23) {
      notifications.push({ 
        title: "⚠️ Cold Temperature Alert!", 
        message: `Your aquarium '${aquariumName}' temperature is too Cold (${temperature}°C)!` });
    }

    // Send notifications and log them
    for (const { title, message } of notifications) {
      const notificationResult = await sendNotification(deviceToken, title, message);
      if (notificationResult.success) {
        await firestore.collection("users").doc(userId).collection("notifications").add({
          title,
          message,
          aquariumName,
          createdAt: Timestamp.now(),
          isRead: false,
        });
        console.log(`✅ Notification saved in Firestore: ${title}`);
      } else {
        return new Response(JSON.stringify({ error: "Notification failed", details: notificationResult.error }), { status: 500 });
      }
    }

    return new Response(JSON.stringify({ status: "Notifications sent and logged" }), { status: 200 });
  } catch (error) {
    console.error("❌ Internal Server Error:", error);
    return new Response(JSON.stringify({ error: "Internal Server Error" }), { status: 500 });
  }
});


