import sys
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

# Get email and verification code from command line arguments
if len(sys.argv) < 3: 
    print("Usage: send_email.py <email> <code>")
    sys.exit(1)

receiver_email = sys.argv[1]
verification_code = sys.argv[2]

# Gmail email and app password configuration
SENDER_EMAIL = "ergprogramm1@gmail.com"  # Replace with your Gmail email
SENDER_PASSWORD = "fbrmkagmugoesinv"  # Replace with your 16-digit app password

# Email message content
subject = "Your Verification Code"
body = f"Your verification code is: {verification_code}"

# Create the email
message = MIMEMultipart()
message["From"] = SENDER_EMAIL
message["To"] = receiver_email
message["Subject"] = subject
message.attach(MIMEText(body, "plain"))

try:
    # Set up the SMTP server
    server = smtplib.SMTP_SSL("smtp.gmail.com", 465)  # Use SSL on port 465
    server.login(SENDER_EMAIL, SENDER_PASSWORD)
    
    # Send email
    server.sendmail(SENDER_EMAIL, receiver_email, message.as_string())
    server.quit()

    print(f"Verification code sent to {receiver_email}")
except Exception as e:
    print(f"Error: {e}")
