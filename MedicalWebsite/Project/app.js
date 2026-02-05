//------------------------------------table highlight on hover-------------------------------------------

// Select all table rows in the document and add hover effects
document.querySelectorAll("table tbody tr").forEach(row => {
    // Change background color to light green when mouse hovers over row
    row.addEventListener("mouseover", () => row.style.backgroundColor = "#bbd7b5");
    // Remove background color when mouse leaves the row
    row.addEventListener("mouseout", () => row.style.backgroundColor = "");
});

//----------------------------------research upload functionality----------------------------
// Get reference to the research upload form
const researchUploadForm = document.getElementById('uploadForm');
if (researchUploadForm) {
    // Add submit event listener to the form
    researchUploadForm.addEventListener('submit', function(e) {
        e.preventDefault(); // Prevent default form submission behavior
        
        // Get reference to confirmation message div and show it
        const confirmationDiv = document.getElementById('upload-confirmation');
        confirmationDiv.style.display = 'block';
        
        // Hide the upload form after submission
        researchUploadForm.style.display = 'none';
    });
}

//----------------------------------Image upload functionality------------------------------
// Get references to the upload form and gallery grid
const uploadForm = document.getElementById('uploadForm');
const galleryGrid = document.querySelector('.gallery-grid');

if (uploadForm) {
    // Add submit event listener to the form
    uploadForm.addEventListener('submit', function(e) {
        e.preventDefault(); // Prevent default form submission
        
        // Get references to form input elements
        const imageFile = document.getElementById('imageFile');
        const imageTitle = document.getElementById('imageTitle');
        const imageDescription = document.getElementById('imageDescription');

        // Check if an image file was selected
        if (imageFile.files.length === 0) {
            alert('Please select an image file');
            return;
        }

        // Get the selected file and create a FileReader
        const file = imageFile.files[0];
        const reader = new FileReader();

        // Define what happens when the file is loaded
        reader.onload = function(e) {
            // Create container for the new gallery item
            const container = document.createElement('div');
            container.className = 'gallery-item';

            // Create and configure the image element
            const img = document.createElement('img');
            img.src = e.target.result; // Set image source to the loaded file
            img.alt = imageTitle.value; // Set alt text to the title

            // Create caption container
            const caption = document.createElement('div');
            caption.className = 'gallery-caption';

            // Create and add title element
            const title = document.createElement('h3');
            title.textContent = imageTitle.value;

            // Create and add description element
            const description = document.createElement('p');
            description.textContent = imageDescription.value;

            // Assemble the gallery item structure
            caption.appendChild(title);
            caption.appendChild(description);
            container.appendChild(img);
            container.appendChild(caption);

            // Add the new gallery item to the grid
            galleryGrid.appendChild(container);

            // Reset the form for next upload
            uploadForm.reset();
        };

        // Start reading the file as a data URL
        reader.readAsDataURL(file);
    });
}

//------------------------------------Medical News Ticker Functionality--------------------------------
// Define RSS feed URL and proxy service URL
const rssUrl = 'https://medicalxpress.com/rss-feed/';
const proxyUrl = 'https://api.rss2json.com/v1/api.json?rss_url=';
const ticker = document.getElementById('ticker');
let position = window.innerWidth; // Starting position for ticker animation

// Function to fetch and display news links
async function fetchNewsLinks() {
    // Fetch RSS feed data through proxy
    const res = await fetch(proxyUrl + encodeURIComponent(rssUrl));
    const data = await res.json();

    // Process first 10 news items
    data.items.slice(0, 10).forEach(item => {
        // Create link element for each news item
        const link = document.createElement('a');
        link.href = item.link;
        link.textContent = item.title + ' ✦ '; // Add separator between items
        link.style.marginRight = '30px';
        link.style.color = 'inherit';
        link.style.textDecoration = 'none';
        link.target = '_blank'; // Open links in new tab
        
        // Add hover effects to links
        link.addEventListener("mouseover",function() { 
            link.style.textDecoration = 'underline'; 
        });
        link.addEventListener("mouseout",function() { 
            link.style.textDecoration = 'none';
        });
        
        // Add link to ticker
        ticker.appendChild(link);
    });
}

// Function to animate the ticker
function moveTicker() {
    position -= 1; // Move ticker left by 1 pixel
    ticker.style.transform = `translateX(${position}px)`;
    // Reset position when ticker moves completely off screen
    if (position < -ticker.offsetWidth) position = window.innerWidth;
    requestAnimationFrame(moveTicker); // Continue animation
}

// Start the ticker functionality
fetchNewsLinks().then(moveTicker);
// Refresh news every hour
setInterval(() => {
    ticker.innerHTML = '';
    fetchNewsLinks(); 
}, 3600000);

//-----------------------------------Comment functionality---------------------------
// Function to handle comment submission
function addComment(event) {
    // Get the form that contains the clicked button
    const form = event.target.closest('.comment-form');
    
    // Get input values from the form
    const name = form.querySelector('input[type="text"]').value;
    const email = form.querySelector('input[type="email"]').value;
    const comment = form.querySelector('textarea').value;

    // Validate inputs
    let error = false;
    
    // Validate name (minimum 3 characters)
    if(!name || name.length < 3){
        alert("Please enter your name (at least 3 characters)!");
        error = true;
    }
    
    // Validate comment (minimum 3 characters)
    if(!comment || comment.length < 3){
        alert("Please enter a comment (at least 3 characters)!");
        error = true;
    }
    
    // Validate email format
    if(!email || email.length < 3 || email.indexOf("@") < 0){
        alert("Please enter a valid email address!");
        error = true;
    }

    // Stop if validation failed
    if(error) {
        return;
    }

    // Get the comments section
    const commentSection = form.nextElementSibling;
    // Create new comment element
    const newComment = document.createElement('div');
    newComment.className = 'comment';
    // Set comment HTML structure
    newComment.innerHTML = `
        <div class="comment-header">
            <strong>${name}</strong>
        </div>
        <div class="comment-body">${comment}</div>
    `;

    // Add new comment to the section
    commentSection.appendChild(newComment);

    // Clear the form
    form.querySelector('input[type="text"]').value = '';
    form.querySelector('input[type="email"]').value = '';
    form.querySelector('textarea').value = '';
}

// Add click event listener to all comment buttons
document.querySelectorAll('.commentbtn').forEach(btn => btn.onclick = addComment);

//----------------------------darkmode functionality------------------------------------
// Track current theme state
let darkMode = false;

// Add click handler to theme toggle button
document.getElementById("theme-toggle").onclick= function(){
    // Toggle dark mode state
    darkMode=!darkMode;

    // Log current mode to console
    let msg=darkMode? "dark Mode": "light Mode";
    
    
    // Select all containers that need theme changes
    let containers = document.querySelectorAll('.discovery-card, .content-section, .gallery-item, .upload-section, .card, .comment-section, .links-category');
    
    // Apply dark mode styles
    if(darkMode){
        document.body.style.backgroundColor = 'black';
        containers.forEach(container => {
            container.style.backgroundColor = 'lightgrey';
        });
        updateThemeIcon();
    }else{
        // Apply light mode styles
        document.body.style.backgroundColor = 'white';
        containers.forEach(container => {
            container.style.backgroundColor = 'white';
        });
        updateThemeIcon();
    }
};

// Function to update the theme icon based on current mode
function updateThemeIcon() {
    const icon = document.getElementsByClassName('theme-icon')[0];
    icon.textContent = darkMode == true ? '🌞' : '🌙';
}


