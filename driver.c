#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

// Module metadata
MODULE_AUTHOR("lumenthi");
MODULE_DESCRIPTION("Controller");
MODULE_LICENSE("GPL");

/* Define these values to match your devices */
#define USB_SKEL_VENDOR_ID	0x20d6
#define USB_SKEL_PRODUCT_ID	0xa711
#define USB_SKEL_MINOR_BASE	192

/* table of devices that work with this driver */
static struct usb_device_id skel_table [] = {
	{ USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, skel_table);

static struct usb_driver skel_driver; 

/* Structure to hold all of our device specific stuff */
struct usb_skel {
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct semaphore	limit_sem;		/* limiting the number of writes in progress */
	struct usb_anchor	submitted;		/* in case we need to retract our submissions */
	struct urb		*bulk_in_urb;		/* the urb to read data with */
	unsigned char           *bulk_in_buffer;	/* the buffer to receive data */
	size_t			bulk_in_size;		/* the size of the receive buffer */
	size_t			bulk_in_filled;		/* number of bytes in the buffer */
	size_t			bulk_in_copied;		/* already copied to user space */
	__u8			int_in_endpointAddr;	/* the address of the bulk in endpoint */
	__u8			int_out_endpointAddr;	/* the address of the bulk out endpoint */
	int			errors;			/* the last request tanked */
	int			open_count;		/* count the number of openers */
	bool			ongoing_read;		/* a read is going on */
	bool			processed_urb;		/* indicates we haven't processed the urb */
	spinlock_t		err_lock;		/* lock for errors */
	struct kref		kref;
	struct mutex		io_mutex;		/* synchronize I/O with disconnect */
	struct completion	bulk_in_completion;	/* to wait for an ongoing read */
	
	struct urb 			*int_in_urb;
	size_t				int_in_size;
	struct urb 			*int_out_urb;
	struct usb_endpoint_descriptor *int_in_endpoint;
	struct usb_endpoint_descriptor *int_out_endpoint;
	unsigned char           	*int_in_buffer;
	unsigned char           	*int_out_buffer;
};

static void int_in_callback(struct urb *urb)
{
	struct usb_skel *dev = urb->context;
	
	// printk(KERN_INFO "[+][+][+] Interrupt in callback\n");
	if (urb->status)
		if (urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN)
			printk(KERN_INFO "Error submitting in urb\n");
	// printk(KERN_INFO "[~] Length: %d\n", urb->actual_length);
	if (urb->actual_length > 0) {
		// printk(KERN_INFO "[~] Received data: 0x%x\n", *(int *)dev->int_in_buffer);
	}
	dev->int_in_size = urb->actual_length;

	/* Resubmitting urb */
	usb_submit_urb(dev->int_in_urb, GFP_ATOMIC);
}

static int skel_open(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;
	
	printk(KERN_INFO "[~] Opening the device for I/O\n");
	subminor = iminor(inode);
	interface = usb_find_interface(&skel_driver, subminor);
	if (!interface) {
		printk(KERN_INFO "[!] Error, cant find device for minor: %d\n", subminor);
		return -ENODEV;
	}
	dev = usb_get_intfdata(interface);
	if (!dev) {
		printk(KERN_INFO "[!] Error, cant get dev structure\n");
		return -ENODEV;
	}

	/* increment our usage count for the device */
	kref_get(&dev->kref);
	/* save our object in the file's private structure */
	file->private_data = dev;
	
	/* Filling interrupt out urb */
	printk(KERN_INFO "[~] Submitting the interrupt in urb\n");
	usb_fill_int_urb(dev->int_in_urb,
			 dev->udev,
	                 usb_rcvintpipe(dev->udev, dev->int_in_endpoint->bEndpointAddress),
	                 dev->int_in_buffer,
	                 le16_to_cpu(dev->int_in_endpoint->wMaxPacketSize),
	                 int_in_callback,
	                 dev,
	                 dev->int_in_endpoint->bInterval);

	retval = usb_submit_urb(dev->int_in_urb, GFP_KERNEL);
	if (retval != 0)
		printk(KERN_INFO "[!] Error while submitting the in urb\n");
	return 0;
}

static int skel_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t skel_read(struct file *file, char *user_buffer, size_t count, loff_t *ppos)
{
	struct usb_skel *dev;

	// printk(KERN_INFO "[~] Reading the I/0 device\n");
	dev = (struct usb_skel *)file->private_data;
	if (copy_to_user(user_buffer, dev->int_in_buffer, dev->int_in_size)) {
		printk(KERN_INFO "[!] Copy_to_user error when reading the device\n");
	}
	return dev->int_in_size;
}

static ssize_t skel_write(struct file *file, const char *user_buffer, size_t count, loff_t *ppos)
{
	printk(KERN_INFO "[~] Writing the I/0 device\n");
	return 0;
}

static struct file_operations skel_fops = {
	.owner =	THIS_MODULE,
	.read =		skel_read,
	.write =	skel_write,
	.open =		skel_open,
	.release =	skel_release,
};

char	*device_permissions(struct device *dev, umode_t *mode)
{
	if (mode)
		(*mode) = 0666; /* RW */
	return kasprintf(GFP_KERNEL, "usb/%s", dev_name(dev));
}

static struct usb_class_driver skel_class = {
	.name =		"skel%d",
	.devnode = 	device_permissions,
	.fops =		&skel_fops,
	.minor_base =	USB_SKEL_MINOR_BASE,
};

static void int_out_callback(struct urb *urb)
{
	printk(KERN_INFO "Submitting out urb\n");
	if (urb->status)
		if (urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN)
			printk(KERN_INFO "Error submitting out urb\n");
}

int __init init_usb(void)
{
        int result;

	printk(KERN_INFO "[~] Registering USB driver in the subsystem.\n");
        result = usb_register(&skel_driver);
        if (result < 0)
	{
                printk(KERN_INFO "[!] usb_register failed, code: %d\n", result);
                return -1;
        }
	return 0;
}

/*
	usb_register_dev creates a USB device.
	The major device number of the USB device is 180, and the minor device number determines each different device.
	usb_register_de is generally called in the probe function.
*/

#define WRITES_IN_FLIGHT 8

static int skel_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_skel 		*dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	size_t buffer_size;
	int i;
	int retval = -ENOMEM;

	printk(KERN_INFO "[+][+][+] Device plugged in.\n");

	/* allocate memory for our device state and initialize it */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		printk(KERN_INFO "[!] Out of memory");
		// TODO: free dev when error 
		return -1;
	}

	/* init structure members */
	kref_init(&dev->kref); 
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* set up the endpoint information */
	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;
	printk(KERN_INFO "[~] Number of endpoints to bind: %d\n", iface_desc->desc.bNumEndpoints);
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;
		// must use usb_endpoint_is_int_in(endpoint)
		if (!dev->int_in_endpointAddr &&
		    usb_endpoint_is_int_in(endpoint)) {
			printk(KERN_INFO "[~] Biding interrupt in endpoint\n");
			/* we found an interrupt in endpoint */
			buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			dev->int_in_size = buffer_size;
			dev->int_in_endpoint = endpoint;
			dev->int_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->int_in_buffer) {
				printk(KERN_INFO "[!] Could not allocate int_in_buffer");
				return -1;
			}
		}

		if (!dev->int_out_endpointAddr &&
		    usb_endpoint_is_int_out(endpoint)) {
			printk(KERN_INFO "[~] Biding interrupt out endpoint\n");
			dev->int_out_endpoint = endpoint;
		}
	}
	if (!(dev->int_in_endpoint && dev->int_out_endpoint)) {
		printk(KERN_INFO "[!] Could not find both int-in and int-out endpoints\n");
		return -1;
	}

	printk(KERN_INFO "[~] Allocating int urb\n");
	dev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->int_in_urb) {
		printk(KERN_INFO "[!] Failed usb alloc urb for in interrupt endpoint\n");
		return -1;
	}
	printk(KERN_INFO "[~] Allocating out urb\n");
	dev->int_out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->int_out_urb) {
		printk(KERN_INFO "[!] Failed usb alloc urb for out interrupt endpoint\n");
		return -1;
	}

	/* Filling interrupt out urb */
	usb_fill_int_urb(dev->int_out_urb,
			 dev->udev,
	                 usb_sndintpipe(dev->udev,dev->int_out_endpoint->bEndpointAddress),
	                 dev->int_out_buffer,
	                 buffer_size,
	                 int_out_callback,
	                 dev,
	                 dev->int_out_endpoint->bInterval);

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);	
	/* register the device */
	retval = usb_register_dev(interface, &skel_class);
	if (retval)
	{
		printk(KERN_INFO "[-][-][-] Not able to get a minor for this device.\n");
	}
	printk(KERN_INFO "[+][+][+] USB skeleton device attached to minor: skel%d.\n", interface->minor);
	return 0;
}

static void skel_disconnect(struct usb_interface *interface)
{
	struct usb_skel *dev;

	dev = usb_get_intfdata(interface);

	/* Freeing urbs */
	if (dev->int_in_urb) {
		printk(KERN_INFO "[-] Freeing interrupt in urb\n");
		usb_free_urb(dev->int_in_urb);
	}
	if (dev->int_out_urb) {
		printk(KERN_INFO "[-] Freeing interrupt out urb\n");
		usb_free_urb(dev->int_out_urb);
	}

	usb_deregister_dev(interface, &skel_class);
	dev->interface = NULL;
	kfree(dev);
	printk(KERN_INFO "[-][-][-] Device unplugged.\n");
}

static struct usb_driver skel_driver = {
	.name =		"skeleton",
	.probe =	skel_probe,
	.disconnect =	skel_disconnect,
	.id_table =	skel_table,
};

void __exit cleanup_usb(void)
{
	printk(KERN_INFO "Unregister the USB driver.\n");
        usb_deregister(&skel_driver);
}

module_init(init_usb);
module_exit(cleanup_usb);
