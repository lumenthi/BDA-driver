#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/input.h>
#include <linux/usb/input.h>

/* Module metadata */
MODULE_AUTHOR("lumenthi");
MODULE_DESCRIPTION("BDA Controller");
MODULE_LICENSE("GPL");

/* Values that matches the device */
#define USB_SKEL_VENDOR_ID	0x20d6
#define USB_SKEL_PRODUCT_ID	0xa711
#define USB_SKEL_MINOR_BASE	192

/* Table of devices that work with this driver */
static struct usb_device_id skel_table [] = {
	{ USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, skel_table);

static struct usb_driver skel_driver; 

/* Structure to hold all of our device specific stuff */
struct usb_skel {
	struct usb_device		*udev;
	struct usb_interface		*interface;
	struct kref			kref;

	struct input_dev 		*idev;
	char 				phys[64];

	size_t				int_in_size;
	struct urb 			*int_in_urb;
	struct urb 			*int_out_urb;

	struct usb_endpoint_descriptor *int_in_endpoint;
	struct usb_endpoint_descriptor *int_out_endpoint;

	unsigned char           	*int_in_buffer;
	unsigned char           	*int_out_buffer;
};

/* Common buttons */
static const signed short bda_buttons[] = {BTN_A,BTN_B,BTN_X,BTN_Y,-1};
/* Special buttons */
/*				             HOME    PLUS       LESS         PAIR */
static const signed short bda_sbuttons[] = {BTN_MODE,BTN_START,BTN_SELECT,BTN_EXTRA,-1};
/* Triggers */
static const signed short bda_triggers[] = {BTN_TL,BTN_TR,BTN_TL2,BTN_TR2,-1};
/* Joysticks */
static const signed short bda_joysticks[] = {ABS_X, ABS_Y,ABS_RX, ABS_RY,-1};
/* Joysticks Buttons */
static const signed short bda_jbuttons[] = {BTN_THUMBL,BTN_THUMBR,-1};
/* D-Pad */
/*					     UP                RIGHT              DOWN                LEFT */
static const signed short bda_dpad[] = {BTN_TRIGGER_HAPPY1,BTN_TRIGGER_HAPPY2,BTN_TRIGGER_HAPPY3,BTN_TRIGGER_HAPPY4, -1};

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

static void process_packet(struct usb_skel *dev, struct urb *urb, unsigned char *data)
{

	// printk(KERN_INFO "[~] Processing packet: 0x%llx\n", *(uint64_t*)data);

	/* Reporting joysticks axis */
	input_report_abs(dev->idev, ABS_X, ((*(uint64_t*)data>>24) & 0xFF));
	input_report_abs(dev->idev, ABS_Y, ((*(uint64_t*)data>>32) & 0xFF));
	input_report_abs(dev->idev, ABS_RX, ((*(uint64_t*)data>>40) & 0xFF));
	input_report_abs(dev->idev, ABS_RY, ((*(uint64_t*)data>>48) & 0xFF));

	/* Reporting common buttons */
	input_report_key(dev->idev, BTN_Y, CHECK_BIT(*(uint64_t*)data, 0));
	input_report_key(dev->idev, BTN_B, CHECK_BIT(*(uint64_t*)data, 1));
	input_report_key(dev->idev, BTN_A, CHECK_BIT(*(uint64_t*)data, 2));
	input_report_key(dev->idev, BTN_X, CHECK_BIT(*(uint64_t*)data, 3));

	/* Reporting triggers */
	input_report_key(dev->idev, BTN_TL, CHECK_BIT(*(uint64_t*)data, 4));
	input_report_key(dev->idev, BTN_TR, CHECK_BIT(*(uint64_t*)data, 5));
	input_report_key(dev->idev, BTN_TL2, CHECK_BIT(*(uint64_t*)data, 6));
	input_report_key(dev->idev, BTN_TR2, CHECK_BIT(*(uint64_t*)data, 7));

	/* Reporting joysticks buttons */
	input_report_key(dev->idev, BTN_THUMBL, CHECK_BIT(*(uint64_t*)data, 10));
	input_report_key(dev->idev, BTN_THUMBR, CHECK_BIT(*(uint64_t*)data, 11));

	/* Reporting D-PAD */
	input_report_key(dev->idev, BTN_TRIGGER_HAPPY1, ((*(uint64_t*)data & 0xF0000)>>16 == 0x0 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x1 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x7));
	input_report_key(dev->idev, BTN_TRIGGER_HAPPY2, ((*(uint64_t*)data & 0xF0000)>>16 == 0x1 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x2 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x3));
	input_report_key(dev->idev, BTN_TRIGGER_HAPPY3, ((*(uint64_t*)data & 0xF0000)>>16 == 0x3 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x4 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x5));
	input_report_key(dev->idev, BTN_TRIGGER_HAPPY4, ((*(uint64_t*)data & 0xF0000)>>16 == 0x5 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x6 ||
							 (*(uint64_t*)data & 0xF0000)>>16 == 0x7));

	/* Reporting special buttons */
	input_report_key(dev->idev, BTN_SELECT, CHECK_BIT(*(uint64_t*)data, 8));
	input_report_key(dev->idev, BTN_START, CHECK_BIT(*(uint64_t*)data, 9));
	input_report_key(dev->idev, BTN_MODE, CHECK_BIT(*(uint64_t*)data, 12));
	input_report_key(dev->idev, BTN_EXTRA, CHECK_BIT(*(uint64_t*)data, 13));

	input_sync(dev->idev);
}

static void int_in_callback(struct urb *urb)
{
	struct usb_skel *dev = urb->context;
	
	// printk(KERN_INFO "[+][+][+] Interrupt in callback\n");
	if (urb->status)
		if (urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN) {
			printk(KERN_INFO "[!] Interrupt in urb ended\n");
			return;
		}
	// printk(KERN_INFO "[~] Length: %d\n", urb->actual_length);
	if (urb->actual_length > 0) {
		// printk(KERN_INFO "[~] Received data: 0x%x\n", *(int *)dev->int_in_buffer);
	}
	dev->int_in_size = urb->actual_length;

	process_packet(dev, urb, dev->int_in_buffer);

	/* Resubmitting urb */
	usb_submit_urb(dev->int_in_urb, GFP_ATOMIC);
}

static int start_input(struct usb_skel *dev)
{
	int retval;

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

	retval = usb_submit_urb(dev->int_in_urb, GFP_ATOMIC);

	/* An urb is already submitted */
	if (retval != 0)
		printk(KERN_INFO "[!] Error while submitting the in urb\n");

	return retval;
}

static int skel_open(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;
	struct usb_interface *interface;
	int subminor;
	
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

	/* Increment our usage count for the device */
	kref_get(&dev->kref);
	/* Save our object in the file's private structure */
	file->private_data = dev;

	// printk(KERN_INFO "%d\n", refcount_read(&(&dev->kref)->refcount));

	return start_input(dev);
}

#define to_skel_dev(d) container_of(d, struct usb_skel, kref)

static void skel_delete(struct kref *kref)
{
	struct usb_skel *dev = to_skel_dev(kref);

	printk(KERN_INFO "[~] Deletion routine\n");

	usb_put_dev(dev->udev);
	kfree(dev);
}

/* Called when user uses close() syscall */

static int skel_release(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;

	dev = file->private_data;
	if (dev == NULL) {
		printk(KERN_INFO "[!] Struct usb_skel is empty\n");
		return 1;
	}

	printk(KERN_INFO "[~] Releasing the I/O device\n");
	printk(KERN_INFO "[~] Remaining handlers: %d\n", refcount_read(&(&dev->kref)->refcount));

	if (file->f_mode & FMODE_READ) {
		/* The reference count got incremented 2 times by kref_init() and kref_get() */
		if (dev->int_in_urb && refcount_read(&(&dev->kref)->refcount) <= 2) {
			printk(KERN_INFO "[~] Killing the remaining urb\n");
			usb_kill_urb(dev->int_in_urb);
		}
	}

	/* decrement the count on our device */
	kref_put(&dev->kref, skel_delete);
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
	printk(KERN_INFO "[~] Submitting out urb\n");
	if (urb->status)
		if (urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN) {
			printk(KERN_INFO "[!] Error submitting out urb\n");
			return;
		}
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

static int error_handler(struct usb_skel **u_dev)
{
	struct usb_skel *dev = *u_dev;

	/* Free urbs if allocated */
	if (dev->int_in_urb)
		usb_free_urb(dev->int_in_urb);

	if (dev->int_out_urb)
		usb_free_urb(dev->int_out_urb);

	/* Free allocated memory */
	if (dev)
		kref_put(&dev->kref, skel_delete);

	return -1;
}

static int skel_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_skel 		*dev;
	struct usb_host_interface 	*iface_desc;
	struct usb_endpoint_descriptor 	*endpoint;
	struct	input_dev		*input_dev;
	size_t 				buffer_size;
	int 				i;
	int 				retval = -ENOMEM;

	printk(KERN_INFO "[+][+][+] Device plugged in.\n");

	/* Allocate memory for our device state and initialize it */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		printk(KERN_INFO "[!] Out of memory");
		return error_handler(&dev);
	}

	/* Init structure members */
	kref_init(&dev->kref); 
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* Init input structure */
	printk(KERN_INFO "[~] Allocating input device\n"); /* ls /sys/class/input */
							   /* ls /dev/input */
	input_dev = input_allocate_device();
	if (!input_dev) {
		printk(KERN_INFO "[!] Can't allocate input device\n");
		/* TODO: Handle input_structure idev in error handler */
		return error_handler(&dev);
	}

	dev->idev = input_dev;

	input_dev->name = "BDA Controller";
	input_dev->phys = dev->phys;

	input_dev->id.vendor = USB_SKEL_VENDOR_ID;
	input_dev->id.product = USB_SKEL_PRODUCT_ID;

	usb_to_input_id(dev->udev, &input_dev->id);

	for (i = 0; bda_joysticks[i] >= 0; i++)
		input_set_abs_params(input_dev, bda_joysticks[i], 0x0, 0xff, 0, 0);

	for (i = 0; bda_buttons[i] >= 0; i++)
		input_set_capability(input_dev, EV_KEY, bda_buttons[i]);

	for (i = 0; bda_sbuttons[i] >= 0; i++)
		input_set_capability(input_dev, EV_KEY, bda_sbuttons[i]);

	for (i = 0; bda_triggers[i] >= 0; i++)
		input_set_capability(input_dev, EV_KEY, bda_triggers[i]);

	for (i = 0; bda_jbuttons[i] >= 0; i++)
		input_set_capability(input_dev, EV_KEY, bda_jbuttons[i]);

	for (i = 0; bda_dpad[i] >= 0; i++)
		input_set_capability(input_dev, EV_KEY, bda_dpad[i]);

	printk(KERN_INFO "Registering input device\n");
	retval = input_register_device(dev->idev);

	if (retval) {
		printk(KERN_INFO "[!] Can't register input device\n");
		/* TODO: If function fails the device must be freed with input_free_device */
		return error_handler(&dev);
	}

	i = 0;
	/* Set up the endpoint information */
	/* Use only the first int-in and int-out endpoints */
	iface_desc = interface->cur_altsetting;
	printk(KERN_INFO "[~] Number of endpoints to bind: %d\n", iface_desc->desc.bNumEndpoints);
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!dev->int_in_endpoint &&
		    usb_endpoint_is_int_in(endpoint)) {
			printk(KERN_INFO "[~] Biding interrupt in endpoint\n");
			buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			dev->int_in_size = buffer_size;
			dev->int_in_endpoint = endpoint;
			dev->int_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->int_in_buffer) {
				printk(KERN_INFO "[!] Could not allocate int_in_buffer");
				return error_handler(&dev);
			}
		}

		if (!dev->int_out_endpoint &&
		    usb_endpoint_is_int_out(endpoint)) {
			printk(KERN_INFO "[~] Biding interrupt out endpoint\n");
			dev->int_out_endpoint = endpoint;
		}
	}
	if (!(dev->int_in_endpoint && dev->int_out_endpoint)) {
		printk(KERN_INFO "[!] Could not find both int-in and int-out endpoints\n");
		return error_handler(&dev);

	}

	printk(KERN_INFO "[~] Allocating interupt in urb\n");
	dev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->int_in_urb) {
		printk(KERN_INFO "[!] Failed usb alloc urb for in interrupt endpoint\n");
		return error_handler(&dev);
	}
	printk(KERN_INFO "[~] Allocating interupt out urb\n");
	dev->int_out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->int_out_urb) {
		printk(KERN_INFO "[!] Failed usb alloc urb for out interrupt endpoint\n");
		return error_handler(&dev);
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

	/* Save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);	
	/* Register the device */
	retval = usb_register_dev(interface, &skel_class);
	if (retval) {
		printk(KERN_INFO "[-][-][-] Not able to get a minor for this device.\n");
		return error_handler(&dev);
	}
	printk(KERN_INFO "[+][+][+] USB skeleton device attached to minor: usb/skel%d.\n", interface->minor);

	// start_input(dev);

	return 0;
}

static void skel_disconnect(struct usb_interface *interface)
{
	struct usb_skel *dev;

	dev = usb_get_intfdata(interface);

	/* Freeing device */
	if (dev->idev) {
		printk(KERN_INFO "[-] Freeing input device\n");
		input_unregister_device(dev->idev);
	}

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

	/* Decrement the count on our device */
	kref_put(&dev->kref, skel_delete);

	printk(KERN_INFO "[-][-][-] Device unplugged.\n");
}

static struct usb_driver skel_driver = {
	.name =		"BDA Controller",
	.probe =	skel_probe,
	.disconnect =	skel_disconnect,
	.id_table =	skel_table,
};

void __exit cleanup_usb(void)
{
	printk(KERN_INFO "[-] Unregistering the USB driver.\n");
        usb_deregister(&skel_driver);
}

module_init(init_usb);
module_exit(cleanup_usb);
