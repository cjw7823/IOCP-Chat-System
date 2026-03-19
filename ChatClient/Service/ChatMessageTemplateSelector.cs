using ChatClient.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;

namespace ChatClient.Service
{
    public class ChatMessageTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? IncomingTemplate { get; set; }
        public DataTemplate? OutgoingTemplate { get; set; }

        public override DataTemplate? SelectTemplate(object item, DependencyObject container)
        {
            if (item is not ChatMessage message)
                return base.SelectTemplate(item, container);

            return message.IsMine ? OutgoingTemplate : IncomingTemplate;
        }
    }
}
