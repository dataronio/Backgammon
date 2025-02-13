﻿using Backend.Db;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Mail;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Threading.Tasks;

namespace Backend.Mail
{
    public class Mailer
    {
        public static void SendTest()
        {
            const string to = "kristian.ekman.swe@gmail.com";
            const string subj = "Testar SMTP client.";
            const string body = @"<div><img src='https://backgammon.azurewebsites.net/assets/images/facebook.png'></div><p>Test Tjurgränd 47.</p><a href='https://backgammon.azurewebsites.com'>Testar en html länk.<a/>";
            _ = Send(to, subj, body, "");
        }

        internal static async Task Send(string to, string subject, string text, string pw = "")
        {
            // I trust my own self signed certificate on my smtp server.
            ServicePointManager.ServerCertificateValidationCallback =
            delegate (
                object s,
                X509Certificate certificate,
                X509Chain chain,
                SslPolicyErrors sslPolicyErrors
            )
            {
                return true;
            };

            using (var client = new SmtpClient("smtp1.kristianekman.com", 2525))
            {
                string from = "backgammon@kristianekman.com";
                var message = new MailMessage(from, to, subject, text)
                {
                    IsBodyHtml = true,                    
                };
                client.Credentials = new NetworkCredential("backgammon", pw);
                client.EnableSsl = true;

                client.Send(message);
                await Task.Delay(1000);
            }
        }
    }
}
